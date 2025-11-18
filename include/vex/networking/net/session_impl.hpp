#pragma once

#include <vex/networking/common.hpp>
#include <vex/networking/pdu.hpp>
#include <vex/networking/net/detail/flat_buffer.hpp>
#include <vex/networking/net/threading_policy.hpp>
#include <vex/networking/net/session_metrics.hpp>
#include <vex/networking/net/session_state.hpp>
#include <vex/networking/net/session_config.hpp>
#include <vex/networking/net/error_handler.hpp>
#include <vex/networking/net/protocol_handler.hpp>
#include <vex/networking/net/backpressure_controller.hpp>

#include <boost/asio.hpp>
#include <fmt/core.h>

#include <memory>
#include <vector>
#include <functional>
#include <optional>
#include <array>
#include <span>

namespace pa::pinex
{

// ============================================================================
// Session Implementation (with threading policy)
// ============================================================================

template<typename ThreadingPolicy = PINEX_THREADING_MODE>
class basic_session_impl : public std::enable_shared_from_this<basic_session_impl<ThreadingPolicy>>
{
public:
    using executor_type = typename ThreadingPolicy::executor_type;
    using close_callback = std::function<void(std::shared_ptr<basic_session_impl>, std::optional<std::string>)>;

private:
    static constexpr size_t header_length{10};

    // Configuration
    session_config config_;

    // Threading
    executor_type executor_;

    // Network
    boost::asio::ip::tcp::socket socket_;
    boost::asio::steady_timer unbind_timer_;

    // State
    std::unique_ptr<session_state> state_;
    std::atomic<bool> close_initiated_{false};
    uint32_t sequence_number_{0};

    // Buffers
    std::vector<uint8_t> writing_send_buf_;
    std::vector<uint8_t> pending_send_buf_;
    detail::flat_buffer<uint8_t, 1024 * 1024> receive_buf_;

    // Flow control
    enum class receiving_state { receiving, pending_pause, paused };
    receiving_state receiving_state_{receiving_state::paused};
    backpressure_controller backpressure_;

    // Handlers
    std::unique_ptr<protocol_handler> protocol_handler_;
    std::unique_ptr<error_handler> error_handler_;
    close_callback close_handler_;
    std::function<void()> send_buf_available_handler_;

    // Metrics
    session_metrics metrics_;

public:
    explicit basic_session_impl(boost::asio::io_context& io_context,
                                boost::asio::ip::tcp::socket socket,
                                session_config config = {})
      : config_(std::move(config))
      , executor_(ThreadingPolicy::make_executor(io_context.get_executor()))
      , socket_(std::move(socket))
      , unbind_timer_(executor_)
      , state_(std::make_unique<open_state>())
      , backpressure_(config_.backpressure_low_watermark, config_.backpressure_high_watermark)
      , error_handler_(std::make_unique<logging_error_handler>())
    {
        pending_send_buf_.reserve(config_.send_buf_capacity);
        writing_send_buf_.reserve(config_.send_buf_capacity);
    }

    ~basic_session_impl() = default;

    basic_session_impl(const basic_session_impl&) = delete;
    basic_session_impl& operator=(const basic_session_impl&) = delete;
    basic_session_impl(basic_session_impl&&) = delete;
    basic_session_impl& operator=(basic_session_impl&&) = delete;

    // Configuration
    void set_protocol_handler(std::unique_ptr<protocol_handler> handler)
    {
        protocol_handler_ = std::move(handler);
    }

    void set_error_handler(std::unique_ptr<error_handler> handler)
    {
        error_handler_ = std::move(handler);
    }

    void set_close_handler(close_callback handler)
    {
        close_handler_ = std::move(handler);
    }

    void set_send_buf_available_handler(std::function<void()> handler)
    {
        send_buf_available_handler_ = std::move(handler);
    }

    // Lifecycle
    void start()
    {
        ThreadingPolicy::dispatch(executor_, [self = this->shared_from_this()]() {
            self->do_start();
        });
    }

    void unbind()
    {
        ThreadingPolicy::dispatch(executor_, [self = this->shared_from_this()]() {
            self->do_unbind();
        });
    }

    void close(std::string_view reason)
    {
        bool expected = false;
        if (!close_initiated_.compare_exchange_strong(expected, true))
            return;

        ThreadingPolicy::dispatch(executor_, [self = this->shared_from_this(), reason = std::string(reason)]() {
            self->do_close(reason);
        });
    }

    // Sending
    template<typename PDU>
    void send_response(const PDU& pdu, uint32_t sequence_number, command_status status)
    {
        static_assert(detail::is_response<PDU>, "PDU must be a response");

        ThreadingPolicy::dispatch(executor_, [self = this->shared_from_this(), pdu, sequence_number, status]() {
            self->do_send_impl(pdu, sequence_number, status);
        });
    }

    template<typename PDU>
    uint32_t send_request(const PDU& pdu)
    {
        static_assert(!detail::is_response<PDU>, "PDU must be a request");

        auto seq = next_sequence_number();
        ThreadingPolicy::dispatch(executor_, [self = this->shared_from_this(), pdu, seq]() {
            self->do_send_impl(pdu, seq, command_status::rok);
        });
        return seq;
    }

    // Flow control
    void pause_receiving()
    {
        ThreadingPolicy::dispatch(executor_, [self = this->shared_from_this()]() {
            if (self->receiving_state_ == receiving_state::receiving)
                self->receiving_state_ = receiving_state::pending_pause;
        });
    }

    void resume_receiving()
    {
        ThreadingPolicy::dispatch(executor_, [self = this->shared_from_this()]() {
            self->do_resume_receiving();
        });
    }

    // Queries
    bool is_open() const { return dynamic_cast<open_state*>(state_.get()) != nullptr; }
    const session_metrics& metrics() const { return metrics_; }
    const char* state_name() const { return state_->name(); }

    std::optional<std::tuple<std::string, uint16_t>> remote_endpoint() const noexcept
    {
        try {
            auto endpoint = socket_.remote_endpoint();
            return {{endpoint.address().to_string(), endpoint.port()}};
        }
        catch (const std::exception&) {
            return std::nullopt;
        }
    }

private:
    void do_start()
    {
        do_resume_receiving();
    }

    void do_unbind()
    {
        if (!state_->can_unbind())
            return;

        transition_to(std::make_unique<unbinding_state>());
        send_command(command_id::unbind_req);

        unbind_timer_.expires_after(config_.unbind_timeout);
        unbind_timer_.async_wait([self = this->shared_from_this()](boost::system::error_code ec) {
            if (!ec && dynamic_cast<unbinding_state*>(self->state_.get()))
            {
                self->close("unbind timeout");
            }
        });
    }

    void do_close(std::string_view reason)
    {
        if (dynamic_cast<closed_state*>(state_.get()))
            return;

        receiving_state_ = receiving_state::paused;

        boost::system::error_code ec;
        unbind_timer_.cancel(ec);

        std::optional<std::string> err;
        if (dynamic_cast<open_state*>(state_.get()))
            err = reason;

        socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        socket_.close(ec);

        transition_to(std::make_unique<closed_state>());
        metrics_.is_closed = true;

        auto close_copy = std::move(close_handler_);
        protocol_handler_.reset();
        error_handler_.reset();
        send_buf_available_handler_ = {};
        close_handler_ = {};

        if (close_copy)
        {
            try {
                close_copy(this->shared_from_this(), err);
            }
            catch (const std::exception& e) {
                std::cerr << "Exception in close_handler: " << e.what() << std::endl;
            }
        }
    }

    void transition_to(std::unique_ptr<session_state> new_state)
    {
        if (state_)
            state_->on_exit(*this);
        state_ = std::move(new_state);
        state_->on_enter(*this);
    }

    uint32_t next_sequence_number()
    {
        if (++sequence_number_ == 0)
            sequence_number_ = 1;
        return sequence_number_;
    }

    void do_resume_receiving()
    {
        auto prev = std::exchange(receiving_state_, receiving_state::receiving);
        if (prev == receiving_state::paused)
            do_receive();
    }

    void do_receive();
    void process_message(uint32_t cmd_len, command_id cmd_id, command_status cmd_status, uint32_t seq_num);
    void dispatch_message(command_id cmd_id, command_status cmd_status, uint32_t seq_num, std::span<const uint8_t> body);
    void handle_response(command_id cmd_id, command_status cmd_status, uint32_t seq_num, std::span<const uint8_t> body);
    void handle_request(command_id cmd_id, command_status cmd_status, uint32_t seq_num, std::span<const uint8_t> body);

    template<typename PDU>
    void do_send_impl(const PDU& pdu, uint32_t seq_num, command_status status);

    void send_command(command_id cmd_id, uint32_t seq_num = 0, command_status status = command_status::rok);
    void do_send();

    static bool is_response(command_id cmd_id);
    static std::tuple<uint32_t, command_id, command_status, uint32_t> deserialize_header(std::span<const uint8_t> buf);
    static std::array<uint8_t, header_length> serialize_header(uint32_t cmd_len, command_id cmd_id, uint32_t seq_num, command_status status);
};

// Type aliases
using session_impl = basic_session_impl<PINEX_THREADING_MODE>;
using session_ptr = std::shared_ptr<session_impl>;

} // namespace pa::pinex

// Include implementation
#include <vex/networking/net/session_impl.inl>