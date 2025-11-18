#pragma once

#include <vex/networking/common.hpp>

#include <iostream>
#include <string>
#include <span>

namespace pa::pinex
{

// ============================================================================
// Error Handler Interface
// ============================================================================

class error_handler
{
public:
    virtual ~error_handler() = default;
    virtual void on_deserialization_error(const std::string& msg, command_id id, std::span<const uint8_t> data) = 0;
    virtual void on_protocol_error(const std::string& msg) = 0;
    virtual void on_network_error(const std::string& msg) = 0;
};

// ============================================================================
// Built-in Implementations
// ============================================================================

class logging_error_handler : public error_handler
{
public:
    void on_deserialization_error(const std::string& msg, command_id id, std::span<const uint8_t>) override
    {
        std::cerr << "Deserialization error [cmd=" << static_cast<int>(id) << "]: " << msg << std::endl;
    }

    void on_protocol_error(const std::string& msg) override
    {
        std::cerr << "Protocol error: " << msg << std::endl;
    }

    void on_network_error(const std::string& msg) override
    {
        std::cerr << "Network error: " << msg << std::endl;
    }
};

class silent_error_handler : public error_handler
{
public:
    void on_deserialization_error(const std::string&, command_id, std::span<const uint8_t>) override {}
    void on_protocol_error(const std::string&) override {}
    void on_network_error(const std::string&) override {}
};

class throwing_error_handler : public error_handler
{
public:
    void on_deserialization_error(const std::string& msg, command_id id, std::span<const uint8_t>) override
    {
        throw std::runtime_error("Deserialization error [cmd=" + std::to_string(static_cast<int>(id)) + "]: " + msg);
    }

    void on_protocol_error(const std::string& msg) override
    {
        throw std::runtime_error("Protocol error: " + msg);
    }

    void on_network_error(const std::string& msg) override
    {
        throw std::runtime_error("Network error: " + msg);
    }
};

} // namespace pa::pinex