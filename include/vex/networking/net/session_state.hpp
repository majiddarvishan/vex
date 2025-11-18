#pragma once

namespace pa::pinex
{

// Forward declaration
template<typename ThreadingPolicy>
class basic_session_impl;

// ============================================================================
// Session State Pattern
// ============================================================================

class session_state
{
public:
    virtual ~session_state() = default;
    virtual const char* name() const = 0;
    virtual bool can_send() const = 0;
    virtual bool can_unbind() const = 0;

    template<typename ThreadingPolicy>
    void on_enter(basic_session_impl<ThreadingPolicy>& /*session*/) {}

    template<typename ThreadingPolicy>
    void on_exit(basic_session_impl<ThreadingPolicy>& /*session*/) {}
};

// ============================================================================
// Concrete States
// ============================================================================

class open_state : public session_state
{
public:
    const char* name() const override { return "open"; }
    bool can_send() const override { return true; }
    bool can_unbind() const override { return true; }
};

class unbinding_state : public session_state
{
public:
    const char* name() const override { return "unbinding"; }
    bool can_send() const override { return false; }
    bool can_unbind() const override { return false; }
};

class closed_state : public session_state
{
public:
    const char* name() const override { return "closed"; }
    bool can_send() const override { return false; }
    bool can_unbind() const override { return false; }
};

} // namespace pa::pinex