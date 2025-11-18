#pragma once

#include <boost/asio.hpp>
#include <iostream>

namespace pa::pinex
{

// ============================================================================
// TCP Utilities
// ============================================================================

inline void enable_keepalive(boost::asio::ip::tcp::socket& socket, uint16_t inactivity_timeout)
{
    boost::system::error_code ec;
    socket.set_option(boost::asio::socket_base::keep_alive(true), ec);

    if (ec)
    {
        std::cerr << "Warning: Failed to enable SO_KEEPALIVE: " << ec.message() << std::endl;
        return;
    }

#ifdef __linux__
    int enable = 1;
    int idle = static_cast<int>(inactivity_timeout);
    int interval = 10;
    int count = 5;
    int fd = static_cast<int>(socket.native_handle());

    if (::setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(enable)) != 0)
        std::cerr << "Warning: SO_KEEPALIVE setsockopt failed" << std::endl;

    if (::setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle)) != 0)
        std::cerr << "Warning: TCP_KEEPIDLE setsockopt failed" << std::endl;

    if (::setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval)) != 0)
        std::cerr << "Warning: TCP_KEEPINTVL setsockopt failed" << std::endl;

    if (::setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count)) != 0)
        std::cerr << "Warning: TCP_KEEPCNT setsockopt failed" << std::endl;
#elif defined(_WIN32)
    // Windows implementation
    DWORD bytes_returned = 0;
    tcp_keepalive keepalive_settings{};
    keepalive_settings.onoff = 1;
    keepalive_settings.keepalivetime = inactivity_timeout * 1000;  // milliseconds
    keepalive_settings.keepaliveinterval = 10000;  // 10 seconds

    if (WSAIoctl(socket.native_handle(), SIO_KEEPALIVE_VALS,
                 &keepalive_settings, sizeof(keepalive_settings),
                 nullptr, 0, &bytes_returned, nullptr, nullptr) != 0)
    {
        std::cerr << "Warning: Failed to set TCP keepalive on Windows" << std::endl;
    }
#endif
}

inline void enable_no_delay(boost::asio::ip::tcp::socket& socket)
{
    boost::system::error_code ec;
    socket.set_option(boost::asio::ip::tcp::no_delay(true), ec);

    if (ec)
        std::cerr << "Warning: Failed to enable TCP_NODELAY: " << ec.message() << std::endl;
}

inline void set_recv_buffer_size(boost::asio::ip::tcp::socket& socket, int size)
{
    boost::system::error_code ec;
    socket.set_option(boost::asio::socket_base::receive_buffer_size(size), ec);

    if (ec)
        std::cerr << "Warning: Failed to set receive buffer size: " << ec.message() << std::endl;
}

inline void set_send_buffer_size(boost::asio::ip::tcp::socket& socket, int size)
{
    boost::system::error_code ec;
    socket.set_option(boost::asio::socket_base::send_buffer_size(size), ec);

    if (ec)
        std::cerr << "Warning: Failed to set send buffer size: " << ec.message() << std::endl;
}

} // namespace pa::pinex