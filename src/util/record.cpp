#include <chrono>
#include <ctime>
#include <exception>
#include <iomanip>
#include <iostream>

#include "record.hpp"


/**
 *  @brief Record Line to Designated Log 
 *
 *  @param message:     R-value string message
 *  @param type:        type of message
 *  @param [opt] flush: flush log immediately
 *
 *  @details Write message to appropriate log determined by the type
 *  of message being recorded with the option to flush the buffer upon
 *  writing to buffer.
 */
void
util::log::record
(
    const std::string     &&message,
    const util::log::type   type,
    const bool              flush
) noexcept
{
    const util::clock::time_t time = util::clock::time();

    std::string prefix;
    bool        error_type = false;

    switch (type) 
    {
    // Log types
    case util::log::type::STATUS:
        prefix = "STATUS:";
        break;
    case util::log::type::START:
        prefix = "START:";
        break;
    case util::log::type::STOP:
        prefix = "STOP:";
        break;

    // Error types
    case util::log::type::FLAG:
        prefix = "FLAG:";
        error_type = true;
        break;
    case util::log::type::ERROR:
        prefix = "ERROR:";
        error_type = true;
        break;
    case util::log::type::ABORT:
        prefix = "ABORT:";
        error_type = true;
        break;

    default:
        std::cerr << "ERROR: Unkown log message type" << std::endl;
        return;
    }

    if (error_type)
    {
        std::cerr << time << log::SPACE << prefix << log::SPACE 
            << message << log::NEW_LINE;
        if (flush)
            std::cerr.flush();
    }

    else 
    {
        std::clog << time << log::SPACE << prefix << log::SPACE 
            << message << log::NEW_LINE;
        if (flush)
            std::clog.flush(); 
    }
}


/**
 *  @brief Clock Time Now
 *
 *  @details Capture the immediate time to only to second precision 
 */
util::clock::time_t
static util::clock::time() noexcept
{
    try
    {
        // Time literals
        using std::chrono::time_point;
        time_point   moment = std::chrono::system_clock::now();
        std::time_t  time   = std::chrono::system_clock::to_time_t(moment);
        std::tm     *local  = std::localtime(&time);

        // Precision
        using std::chrono::microseconds;
        using std::chrono::duration_cast;
        microseconds precision 
            = duration_cast<microseconds>(moment.time_since_epoch()) % 1'000'000;

        // Time string
        std::stringstream stream;
        stream << "[" << std::put_time(local, "%Y-%m-%d %H:%M:%S") << "." 
            << std::setw(6) << std::setfill('0') << precision.count() << "]";

        return stream.str();
    }

    catch (const std::exception &exception)
    {
        return "[N/A N/A]";
    }
}
