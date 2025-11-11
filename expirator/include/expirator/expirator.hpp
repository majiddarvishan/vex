#pragma once

/**
 * @file expirator.hpp
 * @brief Main header file for the expirator library
 *
 * Include this file to get access to all expirator implementations
 */

#include <expirator/base_expirator.hpp>
#include <expirator/heap_expirator.hpp>
#include <expirator/timing_wheel_expirator.hpp>
#include <expirator/lockfree_expirator.hpp>

namespace expirator {

/// Version information
inline constexpr int version_major = 1;
inline constexpr int version_minor = 0;
inline constexpr int version_patch = 0;

inline const char* version_string() {
    return "1.0.0";
}

}  // namespace expirator
