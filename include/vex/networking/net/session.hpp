#pragma once

#include <vex/networking/common.hpp>
#include <vex/networking/net/detail/flat_buffer.hpp>
#include <vex/networking/pdu.hpp>

#include <boost/asio.hpp>
#include <fmt/core.h>

#include <memory>
#include <variant>
#include <optional>
#include <iostream>
#include <vector>

namespace pa::pinex
{
static void enable_keepalive(boost::asio::ip::tcp::socket& socket, uint16_t inactivity_timeout)
{
    boost::system::error_code ec;
    socket.set_option(boost::asio::socket_base::keep_alive(true), ec);

    if (ec)
    {
        std::cerr << "Warning: Failed to enable SO_KEEPALIVE: " << ec.message() << std::endl;
    }

    int enable = 1;
    int idle = static_cast<int>(inactivity_timeout);
    int interval = 10;
    int count = 5;
    int fd = static_cast<int>(socket.native_handle());

    if (::setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(enable)) != 0)
    {
        std::cerr << "Warning: SO_KEEPALIVE setsockopt failed" << std::endl;
    }
    if (::setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle)) != 0)
    {
        std::cerr << "Warning: TCP_KEEPIDLE setsockopt failed" << std::endl;
    }
    if (::setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval)) != 0)
    {
        std::cerr << "Warning: TCP_KEEPINTVL setsockopt failed" << std::endl;
    }
    if (::setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count)) != 0)
    {
        std::cerr << "Warning: TCP_KEEPCNT setsockopt failed" << std::endl;
    }
}

using request = std::variant<std::monostate, bind_request, stream_request>;
using response = std::variant<std::monostate, bind_response, stream_response>;

class session : public std::enable_shared_from_this<session>
{
  public:
    // IMPORTANT: Handlers must not capture shared_ptr<session> to avoid circular references
    // Instead, use weak_ptr if you need to reference the session from handlers
    std::function<void(std::shared_ptr<session>, std::optional<std::string>)> close_handler;
    std::function<void(request&&, uint32_t)> request_handler;
    std::function<void(response&&, uint32_t, command_status)> response_handler;
    std::function<void()> send_buf_available_handler;
    std::function<void(const std::string&, command_id, std::span<const uint8_t>)> deserialization_error_handler;

  private:
    enum class state
    {
        open,
        unbinding,
        close,
    };

    enum class receiving_state
    {
        receiving,
        pending_pause,
        paused
    };

    static constexpr size_t header_length{10};
    static constexpr size_t default_send_buf_capacity{1024 * 1024};
    static constexpr size_t small_body_size{256};  // Stack allocation threshold
    static constexpr uint32_t max_command_length{10 * 1024 * 1024};  // 10MB max message
    static constexpr std::chrono::seconds unbind_timeout{5};

    state state_{state::open};
    receiving_state receiving_state_{receiving_state::paused};
    uint32_t sequence_number_{0};
    int inactivity_counter_{};
    boost::asio::ip::tcp::socket socket_;
    boost::asio::steady_timer unbind_timer_;
    std::vector<uint8_t> writing_send_buf_;
    std::vector<uint8_t> pending_send_buf_;
    size_t send_buf_threshold_{1024 * 1024};
    detail::flat_buffer<uint8_t, 1024 * 1024> receive_buf_{};
    std::atomic<bool> close_initiated_{false};

  public:
    explicit session(boost::asio::io_context* io_context, boost::asio::ip::tcp::socket socket)
      : socket_(std::move(socket))
      , unbind_timer_(socket_.get_executor())
    {
        // Reserve capacity to avoid frequent reallocations
        pending_send_buf_.reserve(default_send_buf_capacity);
        writing_send_buf_.reserve(default_send_buf_capacity);
    }

    session(const session&) = delete;
    session& operator=(const session&) = delete;
    session(session&&) = delete;
    session& operator=(session&&) = delete;
    ~session() = default;

    void start()
    {
        resume_receiving();
    }

    std::tuple<std::string, uint16_t> remote_endpoint() const
    {
        auto endpoint = socket_.remote_endpoint();
        return {endpoint.address().to_string(), endpoint.port()};
    }

    std::optional<std::tuple<std::string, uint16_t>> safe_remote_endpoint() const noexcept
    {
        try
        {
            auto endpoint = socket_.remote_endpoint();
            return {{endpoint.address().to_string(), endpoint.port()}};
        }
        catch (const std::exception&)
        {
            return std::nullopt;
        }
    }

    bool is_open() const
    {
        return state_ == state::open;
    }

    void unbind()
    {
        if (state_ == state::open)
        {
            state_ = state::unbinding;
            send_command(command_id::unbind_req);

            // Add timeout for unbind operation
            unbind_timer_.expires_after(unbind_timeout);
            unbind_timer_.async_wait([wptr = weak_from_this()](boost::system::error_code ec) {
                if (auto self = wptr.lock())
                {
                    if (!ec && self->state_ == state::unbinding)
                    {
                        self->close("unbind timeout - no response received");
                    }
                }
            });
        }
    }

    template<typename PDU>
    void send_response(const PDU& pdu, uint32_t sequence_number, command_status command_status)
    {
        static_assert(detail::is_response<PDU>, "PDU isn't a response");
        send_impl(pdu, sequence_number, command_status);
    }

    template<typename PDU>
    uint32_t send_request(const PDU& pdu)
    {
        static_assert(!detail::is_response<PDU>, "PDU isn't a request");
        auto sequence_number = next_sequence_number();
        send_impl(pdu, sequence_number, command_status::rok);
        return sequence_number;
    }

    void set_send_buf_threshold(size_t size)
    {
        send_buf_threshold_ = size;
    }

    bool is_send_buf_above_threshold() const
    {
        return pending_send_buf_.size() > send_buf_threshold_;
    }

    void pause_receiving()
    {
        if (receiving_state_ == receiving_state::receiving)
            receiving_state_ = receiving_state::pending_pause;
    }

    void resume_receiving()
    {
        auto prev_receiving_state = std::exchange(receiving_state_, receiving_state::receiving);
        if (prev_receiving_state == receiving_state::paused)
            do_receive();
    }

  private:
    void close(std::string_view reason)
    {
        // Check close_initiated before posting to avoid race condition
        bool expected = false;
        if (!close_initiated_.compare_exchange_strong(expected, true))
        {
            return; // Already closing
        }

        boost::asio::post(socket_.get_executor(), [wptr = this->weak_from_this(), reason = std::string(reason)]() {
            if (auto self = wptr.lock())
            {
                if (self->state_ == state::close)
                    return;

                self->pause_receiving();

                // Cancel unbind timer if it's running
                boost::system::error_code ec;
                self->unbind_timer_.cancel(ec);

                std::optional<std::string> err;
                if (self->state_ == state::open)
                    err = reason;

                self->socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
                self->socket_.close(ec);
                self->state_ = state::close;

                // Move handlers before clearing to ensure exception safety
                auto close_copy = std::move(self->close_handler);
                self->request_handler = {};
                self->response_handler = {};
                self->send_buf_available_handler = {};
                self->deserialization_error_handler = {};
                self->close_handler = {};

                if (close_copy)
                {
                    try
                    {
                        close_copy(self, err);
                    }
                    catch (const std::exception& e)
                    {
                        std::cerr << "Exception in close_handler: " << e.what() << std::endl;
                    }
                    catch (...)
                    {
                        std::cerr << "Unknown exception in close_handler" << std::endl;
                    }
                }
            }
        });
    }

    uint32_t next_sequence_number()
    {
        // Correct overflow handling: wraps from 0xFFFFFFFF to 1
        if (++sequence_number_ == 0)
            sequence_number_ = 1;
        return sequence_number_;
    }

    void validate_state_for_send() const
    {
        if (state_ == state::close)
            throw std::logic_error{"Send on closed session"};
        if (state_ == state::unbinding)
            throw std::logic_error{"Send on unbinding session"};
    }

    void do_receive()
    {
        while (true)
        {
            if (receiving_state_ != receiving_state::receiving)
                break;

            if (receive_buf_.size() < header_length)
                break;

            auto header_buf = std::span{receive_buf_.begin(), receive_buf_.begin() + header_length};

            auto [command_length, command_id, command_status, sequence_number] = deserialize_header(header_buf);

            // Validate command_length
            if (command_length > max_command_length)
            {
                close(fmt::format("Command length {} exceeds maximum {}", command_length, max_command_length));
                return;
            }

            if (receive_buf_.size() < command_length)
                break;

            // Copy body data to avoid dangling span after consume()
            // Use small buffer optimization for small messages
            auto body_size = command_length - header_length;

            if (body_size <= small_body_size && body_size > 0)
            {
                // Stack allocation for small bodies
                std::array<uint8_t, small_body_size> stack_buf;
                auto body_start = receive_buf_.begin() + header_length;
                std::copy_n(body_start, body_size, stack_buf.begin());

                receive_buf_.consume(command_length);

                std::span<const uint8_t> body_span{stack_buf.data(), body_size};
                if (is_response(command_id))
                {
                    consume_response_pdu(command_id, command_status, sequence_number, body_span);
                }
                else
                {
                    consume_request_pdu(command_id, command_status, sequence_number, body_span);
                }
            }
            else if (body_size > 0)
            {
                // Heap allocation for large bodies
                auto body_start = receive_buf_.begin() + header_length;
                auto body_end = receive_buf_.begin() + command_length;
                std::vector<uint8_t> body_buf(body_start, body_end);

                receive_buf_.consume(command_length);

                if (is_response(command_id))
                {
                    consume_response_pdu(command_id, command_status, sequence_number, body_buf);
                }
                else
                {
                    consume_request_pdu(command_id, command_status, sequence_number, body_buf);
                }
            }
            else
            {
                // Empty body
                receive_buf_.consume(command_length);

                std::span<const uint8_t> empty_span{};
                if (is_response(command_id))
                {
                    consume_response_pdu(command_id, command_status, sequence_number, empty_span);
                }
                else
                {
                    consume_request_pdu(command_id, command_status, sequence_number, empty_span);
                }
            }
        }

        if (receiving_state_ == receiving_state::pending_pause)
        {
            receiving_state_ = receiving_state::paused;
            return;
        }

        socket_.async_receive(receive_buf_.prepare(64 * 1024), [self = shared_from_this()](std::error_code ec, size_t received) {
            if (ec)
                return self->close(ec.message());

            self->receive_buf_.commit(received);

            if (self->state_ == state::open)
                self->inactivity_counter_ = 0;

            self->do_receive();
        });
    }

    void consume_response_pdu(command_id command_id, command_status command_status, uint32_t sequence_number, std::span<const uint8_t> buf)
    {
        response resp;

        try
        {
            switch (command_id)
            {
                case command_id::enquire_link_resp:
                    break;
                case command_id::unbind_resp:
                {
                    // Cancel unbind timer
                    boost::system::error_code ec;
                    unbind_timer_.cancel(ec);
                    close("unbind_resp received");
                    break;
                }
                case command_id::bind_resp:
                    resp = deserialize<bind_response>(buf, bind_type::bi_direction);
                    break;
                case command_id::stream_resp:
                    resp = deserialize<stream_response>(buf);
                    break;
                default:
                    throw std::logic_error{"Unknown pdu"};
            }
        }
        catch (const std::exception& ex)
        {
            if (deserialization_error_handler)
                deserialization_error_handler(std::string{ex.what()}, command_id, buf);

            close(fmt::format("Deserialization exception: {}", ex.what()));
            return;
        }

        if (resp.index() != 0 && state_ == state::open && response_handler)
        {
            try
            {
                response_handler(std::move(resp), sequence_number, command_status);
            }
            catch (const std::exception& e)
            {
                // Post close to executor to avoid calling close while in handler
                auto error_msg = fmt::format("Exception in response_handler: {}", e.what());
                boost::asio::post(socket_.get_executor(), [self = shared_from_this(), msg = std::move(error_msg)]() {
                    self->close(msg);
                });
            }
        }
    }

    void consume_request_pdu(command_id command_id, command_status /* command_status */, uint32_t sequence_number, std::span<const uint8_t> buf)
    {
        request req;

        try
        {
            switch (command_id)
            {
                case command_id::enquire_link_req:
                    send_command(command_id::enquire_link_resp, sequence_number);
                    break;
                case command_id::unbind_req:
                    if (state_ == state::open)
                        state_ = state::unbinding;
                    send_command(command_id::unbind_resp, sequence_number);
                    close("unbind_req received");
                    break;
                case command_id::bind_req:
                    req = deserialize<bind_request>(buf, bind_type::bi_direction);
                    break;
                case command_id::stream_req:
                    req = deserialize<stream_request>(buf);
                    break;
                default:
                    throw std::logic_error{"Unknown pdu"};
            }
        }
        catch (const std::exception& ex)
        {
            if (deserialization_error_handler)
                deserialization_error_handler(std::string{ex.what()}, command_id, buf);

            close(fmt::format("Deserialization exception: {}", ex.what()));
            return;
        }

        if (req.index() != 0 && state_ == state::open && request_handler)
        {
            try
            {
                request_handler(std::move(req), sequence_number);
            }
            catch (const std::exception& e)
            {
                // Post close to executor to avoid calling close while in handler
                auto error_msg = fmt::format("Exception in request_handler: {}", e.what());
                boost::asio::post(socket_.get_executor(), [self = shared_from_this(), msg = std::move(error_msg)]() {
                    self->close(msg);
                });
            }
        }
    }

    template<typename PDU>
    void send_impl(const PDU& pdu, uint32_t sequence_number, command_status command_status)
    {
        validate_state_for_send();

        auto prev_size = pending_send_buf_.size();

        // Reserve space for header (command length unknown until serialization)
        pending_send_buf_.resize(prev_size + header_length);

        try
        {
            serialize_to(&pending_send_buf_, pdu);
        }
        catch (...)
        {
            // Remove appended data due to incomplete serialization
            pending_send_buf_.resize(prev_size);
            throw;
        }

        auto command_length = pending_send_buf_.size() - prev_size;

        auto header = serialize_header(command_length, detail::command_id_of(pdu), sequence_number, command_status);

        std::copy(header.begin(), header.end(), pending_send_buf_.begin() + static_cast<std::ptrdiff_t>(prev_size));

        do_send();
    }

    uint32_t send_command(command_id command_id)
    {
        auto sequence_number = next_sequence_number();
        send_command(command_id, sequence_number);
        return sequence_number;
    }

    void send_command(command_id command_id, uint32_t sequence_number, command_status command_status = command_status::rok)
    {
        auto header = serialize_header(header_length, command_id, sequence_number, command_status);
        pending_send_buf_.insert(pending_send_buf_.end(), header.begin(), header.end());
        do_send();
    }

    void do_send()
    {
        if (!writing_send_buf_.empty())
            return; // Ongoing async_write will call this after completion

        std::swap(writing_send_buf_, pending_send_buf_);

        // Notify if buffer becomes available after being above threshold
        if (writing_send_buf_.size() > send_buf_threshold_ && send_buf_available_handler)
        {
            try
            {
                send_buf_available_handler();
            }
            catch (const std::exception& e)
            {
                std::cerr << "Exception in send_buf_available_handler: " << e.what() << std::endl;
            }
        }

        boost::asio::async_write(socket_, boost::asio::buffer(writing_send_buf_), [self = shared_from_this()](std::error_code ec, size_t) {
            if (ec)
                return self->close(ec.message());

            self->writing_send_buf_.clear();

            if (!self->pending_send_buf_.empty())
                self->do_send();
        });
    }

    static bool is_response(command_id command_id)
    {
        return static_cast<uint8_t>(command_id) & 0x80;
    }

    static std::tuple<uint32_t, command_id, command_status, uint32_t> deserialize_header(std::span<const uint8_t> buf)
    {
        if (buf.size() < header_length)
            throw std::runtime_error{"Invalid header size"};

        auto deserialize_u32 = [](std::span<const uint8_t> b) -> uint32_t {
            if (b.size() < 4)
                throw std::runtime_error{"Invalid u32 buffer size"};
            return static_cast<uint32_t>(b[0]) << 24 |
                   static_cast<uint32_t>(b[1]) << 16 |
                   static_cast<uint32_t>(b[2]) << 8 |
                   static_cast<uint32_t>(b[3]);
        };

        auto command_length = deserialize_u32(buf.subspan(0, 4));
        auto cmd_id = static_cast<pinex::command_id>(buf[4]);
        auto cmd_status = static_cast<pinex::command_status>(buf[5]);
        auto sequence_number = deserialize_u32(buf.subspan(6, 4));

        if (command_length < header_length)
            throw std::runtime_error{"Invalid command_length in header"};

        return {command_length, cmd_id, cmd_status, sequence_number};
    }

    static std::array<uint8_t, header_length> serialize_header(uint32_t command_length, command_id command_id, uint32_t sequence_number, command_status command_status)
    {
        std::array<uint8_t, header_length> buf{};

        auto serialize_u32 = [](std::span<uint8_t> b, uint32_t val) {
            if (b.size() < 4)
                throw std::runtime_error{"Invalid u32 buffer size"};
            b[0] = static_cast<uint8_t>((val >> 24) & 0xFF);
            b[1] = static_cast<uint8_t>((val >> 16) & 0xFF);
            b[2] = static_cast<uint8_t>((val >> 8) & 0xFF);
            b[3] = static_cast<uint8_t>((val >> 0) & 0xFF);
        };

        serialize_u32(std::span{buf}.subspan(0, 4), command_length);
        buf[4] = static_cast<uint8_t>(command_id);
        buf[5] = static_cast<uint8_t>(command_status);
        serialize_u32(std::span{buf}.subspan(6, 4), sequence_number);

        return buf;
    }
};
}  // namespace pa::pinex