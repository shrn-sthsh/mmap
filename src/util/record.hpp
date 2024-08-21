#pragma once

#include <cstdint>
#include <string>


/**
 *  @brief Logging Utility Header
 *
 *  @details Defines data types, flags, and routines related to logging
 */
namespace util
{

namespace log
{

// Types of messages
enum class type: std::uint32_t
{
    STATUS = 0x00,
    ERROR  = 0x01,
    START  = 0x02,
    STOP   = 0x04,
    FLAG   = 0x08,
    ABORT  = 0x10 
};

// Flushing flags
constexpr bool FLUSH = true;
constexpr bool ASYNC = false;

// Record a message to appropriate log
void
record
(
    const std::string &&message,
    const type          type  = type::STATUS,
    const bool          flush = ASYNC
) noexcept;

// String short hands
constexpr char SPACE[]    = " ";
constexpr char NEW_LINE[] = "\n";

} // log namespace

namespace clock
{

using time_t = std::string;

// Get precise time now string
[[nodiscard("Result time string must be used")]]
time_t 
static time() noexcept;

} // clock namespace

} // util namespace
