#pragma once

// Implementation file for basic_session_impl template methods

namespace pa::pinex
{

template<typename ThreadingPolicy>
void basic_session_impl<ThreadingPolicy>::do_receive()
{
    while (true)
    {
        if (receiving_state_ != receiving_state::receiving)
            break;

        if (receive_buf_.size() < header_length)
            break;

        auto header_span = std::span{receive_buf_.begin(), receive_buf_.begin() + header_length};
        auto [cmd_len, cmd_id, cmd_status, seq_num] = deserialize_header(header_span);

        if (cmd_len > config_.max_command_length)
        {
            close(fmt::format("Command length {} exceeds max {}", cmd_len, config_.max_command_length));
            return;
        }

        if (receive_buf_.size() < cmd_len)
            break;

        process_message(cmd_len, cmd_id, cmd_status, seq_num);
    }

    if (receiving_state_ == receiving_state::pending_pause)
    {
        receiving_state_ = receiving_state::paused;
        return;
    }

    socket_.async_receive(receive_buf_.prepare(64 * 1024),
        [self = this->shared_from_this()](boost::system::error_code ec, size_t received) {
            if (ec)
                return self->close(ec.message());

            self->receive_buf_.commit(received);
            self->metrics_.bytes_received += received;
            self->do_receive();
        });
}

template<typename ThreadingPolicy>
void basic_session_impl<ThreadingPolicy>::process_message(uint32_t cmd_len, command_id cmd_id,
                                                           command_status cmd_status, uint32_t seq_num)
{
    auto body_size = cmd_len - header_length;

    if (body_size <= config_.small_body_size && body_size > 0)
    {
        std::array<uint8_t, 256> stack_buf;
        std::copy_n(receive_buf_.begin() + header_length, body_size, stack_buf.begin());
        receive_buf_.consume(cmd_len);

        std::span<const uint8_t> body_span{stack_buf.data(), body_size};
        dispatch_message(cmd_id, cmd_status, seq_num, body_span);
    }
    else if (body_size > 0)
    {
        std::vector<uint8_t> body_buf(receive_buf_.begin() + header_length,
                                     receive_buf_.begin() + cmd_len);
        receive_buf_.consume(cmd_len);
        dispatch_message(cmd_id, cmd_status, seq_num, body_buf);
    }
    else
    {
        receive_buf_.consume(cmd_len);
        std::span<const uint8_t> empty{};
        dispatch_message(cmd_id, cmd_status, seq_num, empty);
    }

    metrics_.messages_received++;
}

template<typename ThreadingPolicy>
void basic_session_impl<ThreadingPolicy>::dispatch_message(command_id cmd_id, command_status cmd_status,
                                                            uint32_t seq_num, std::span<const uint8_t> body)
{
    if (is_response(cmd_id))
        handle_response(cmd_id, cmd_status, seq_num, body);
    else
        handle_request(cmd_id, cmd_status, seq_num, body);
}

template<typename ThreadingPolicy>
void basic_session_impl<ThreadingPolicy>::handle_response(command_id cmd_id, command_status cmd_status,
                                                           uint32_t seq_num, std::span<const uint8_t> body)
{
    try
    {
        response resp;

        switch (cmd_id)
        {
            case command_id::enquire_link_resp:
                break;
            case command_id::unbind_resp:
                unbind_timer_.cancel();
                close("unbind_resp received");
                return;
            case command_id::bind_resp:
                resp = deserialize<bind_response>(body, bind_type::bi_direction);
                break;
            case command_id::stream_resp:
                resp = deserialize<stream_response>(body);
                break;
            default:
                throw std::logic_error{"Unknown response PDU"};
        }

        if (resp.index() != 0 && protocol_handler_)
        {
            try {
                protocol_handler_->on_response(std::move(resp), seq_num, cmd_status);
            }
            catch (const std::exception& e) {
                metrics_.errors++;
                if (error_handler_)
                    error_handler_->on_protocol_error(fmt::format("Handler exception: {}", e.what()));
                close(fmt::format("Exception in response handler: {}", e.what()));
            }
        }
    }
    catch (const std::exception& ex)
    {
        metrics_.errors++;
        if (error_handler_)
            error_handler_->on_deserialization_error(ex.what(), cmd_id, body);
        close(fmt::format("Deserialization error: {}", ex.what()));
    }
}

template<typename ThreadingPolicy>
void basic_session_impl<ThreadingPolicy>::handle_request(command_id cmd_id, command_status cmd_status,
                                                          uint32_t seq_num, std::span<const uint8_t> body)
{
    try
    {
        request req;

        switch (cmd_id)
        {
            case command_id::enquire_link_req:
                send_command(command_id::enquire_link_resp, seq_num);
                break;
            case command_id::unbind_req:
                if (state_->can_unbind())
                    transition_to(std::make_unique<unbinding_state>());
                send_command(command_id::unbind_resp, seq_num);
                close("unbind_req received");
                return;
            case command_id::bind_req:
                req = deserialize<bind_request>(body, bind_type::bi_direction);
                break;
            case command_id::stream_req:
                req = deserialize<stream_request>(body);
                break;
            default:
                throw std::logic_error{"Unknown request PDU"};
        }

        if (req.index() != 0 && protocol_handler_)
        {
            try {
                protocol_handler_->on_request(std::move(req), seq_num);
            }
            catch (const std::exception& e) {
                metrics_.errors++;
                if (error_handler_)
                    error_handler_->on_protocol_error(fmt::format("Handler exception: {}", e.what()));
                close(fmt::format("Exception in request handler: {}", e.what()));
            }
        }
    }
    catch (const std::exception& ex)
    {
        metrics_.errors++;
        if (error_handler_)
            error_handler_->on_deserialization_error(ex.what(), cmd_id, body);
        close(fmt::format("Deserialization error: {}", ex.what()));
    }
}

template<typename ThreadingPolicy>
template<typename PDU>
void basic_session_impl<ThreadingPolicy>::do_send_impl(const PDU& pdu, uint32_t seq_num, command_status status)
{
    if (!state_->can_send())
    {
        if (error_handler_)
            error_handler_->on_protocol_error(
                fmt::format("Cannot send in state: {}", state_->name()));
        return;
    }

    auto prev_size = pending_send_buf_.size();
    pending_send_buf_.resize(prev_size + header_length);

    try {
        serialize_to(&pending_send_buf_, pdu);
    }
    catch (...) {
        pending_send_buf_.resize(prev_size);
        throw;
    }

    auto cmd_len = pending_send_buf_.size() - prev_size;
    auto header = serialize_header(cmd_len, detail::command_id_of(pdu), seq_num, status);
    std::copy(header.begin(), header.end(), pending_send_buf_.begin() + prev_size);

    metrics_.messages_sent++;

    // Check backpressure
    if (backpressure_.should_pause(pending_send_buf_.size()))
        pause_receiving();

    do_send();
}

template<typename ThreadingPolicy>
void basic_session_impl<ThreadingPolicy>::send_command(command_id cmd_id, uint32_t seq_num, command_status status)
{
    if (seq_num == 0)
        seq_num = next_sequence_number();

    auto header = serialize_header(header_length, cmd_id, seq_num, status);
    pending_send_buf_.insert(pending_send_buf_.end(), header.begin(), header.end());

    metrics_.messages_sent++;
    do_send();
}

template<typename ThreadingPolicy>
void basic_session_impl<ThreadingPolicy>::do_send()
{
    if (!writing_send_buf_.empty())
        return;

    std::swap(writing_send_buf_, pending_send_buf_);

    if (backpressure_.should_resume(pending_send_buf_.size()))
    {
        resume_receiving();
        if (send_buf_available_handler_)
        {
            try {
                send_buf_available_handler_();
            }
            catch (const std::exception& e) {
                std::cerr << "Exception in send_buf_available_handler: " << e.what() << std::endl;
            }
        }
    }

    boost::asio::async_write(socket_, boost::asio::buffer(writing_send_buf_),
        [self = this->shared_from_this()](boost::system::error_code ec, size_t sent) {
            if (ec)
                return self->close(ec.message());

            self->metrics_.bytes_sent += sent;
            self->writing_send_buf_.clear();

            if (!self->pending_send_buf_.empty())
                self->do_send();
        });
}

template<typename ThreadingPolicy>
bool basic_session_impl<ThreadingPolicy>::is_response(command_id cmd_id)
{
    return static_cast<uint8_t>(cmd_id) & 0x80;
}

template<typename ThreadingPolicy>
std::tuple<uint32_t, command_id, command_status, uint32_t>
basic_session_impl<ThreadingPolicy>::deserialize_header(std::span<const uint8_t> buf)
{
    if (buf.size() < header_length)
        throw std::runtime_error{"Invalid header size"};

    auto read_u32 = [](std::span<const uint8_t> b) -> uint32_t {
        return static_cast<uint32_t>(b[0]) << 24 |
               static_cast<uint32_t>(b[1]) << 16 |
               static_cast<uint32_t>(b[2]) << 8 |
               static_cast<uint32_t>(b[3]);
    };

    auto cmd_len = read_u32(buf.subspan(0, 4));
    auto cmd_id = static_cast<command_id>(buf[4]);
    auto cmd_status = static_cast<command_status>(buf[5]);
    auto seq_num = read_u32(buf.subspan(6, 4));

    if (cmd_len < header_length)
        throw std::runtime_error{"Invalid command_length"};

    return {cmd_len, cmd_id, cmd_status, seq_num};
}

template<typename ThreadingPolicy>
std::array<uint8_t, basic_session_impl<ThreadingPolicy>::header_length>
basic_session_impl<ThreadingPolicy>::serialize_header(uint32_t cmd_len, command_id cmd_id,
                                                       uint32_t seq_num, command_status status)
{
    std::array<uint8_t, header_length> buf{};

    auto write_u32 = [](std::span<uint8_t> b, uint32_t val) {
        b[0] = static_cast<uint8_t>((val >> 24) & 0xFF);
        b[1] = static_cast<uint8_t>((val >> 16) & 0xFF);
        b[2] = static_cast<uint8_t>((val >> 8) & 0xFF);
        b[3] = static_cast<uint8_t>(val & 0xFF);
    };

    write_u32(std::span{buf}.subspan(0, 4), cmd_len);
    buf[4] = static_cast<uint8_t>(cmd_id);
    buf[5] = static_cast<uint8_t>(status);
    write_u32(std::span{buf}.subspan(6, 4), seq_num);

    return buf;
}

} // namespace pa::pinex