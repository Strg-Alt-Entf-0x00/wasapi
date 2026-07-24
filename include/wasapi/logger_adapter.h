#pragma once
/***************************************************************************
 * WASAPI - Windows Audio Library
 *
 * Optional Logger Adapter: core::Logger integration
 *
 * Bridges wasapi internal logging to the project-wide core::Logger
 * (D:\AI-Projects\third-party-cpp\logger).
 *
 * Usage (once at application startup):
 *
 *   // In your main.cpp or application init:
 *   #include <core/logger.hpp>            // Must come BEFORE this header
 *   #include <wasapi/logger_adapter.h>    // Declares connectCoreLogger()
 *
 *   // After core::Logger is initialized:
 *   wasapi::connectCoreLogger();
 *
 * After this call, all wasapi messages appear in core::Logger output
 * tagged with "[wasapi]" prefix.
 *
 * Thread-Safety: Call connectCoreLogger() once from the main thread
 * before creating any wasapi objects.
 *
 * License: MIT
 ***************************************************************************/

#include <wasapi/logger.h>

// Guard: Verify core/logger.hpp has been included.
// If you get a compile error here, add: #include <core/logger.hpp>
#ifndef CORE_LOGGER_HPP_INCLUDED
    // core/logger.hpp uses #pragma once but defines no macro.
    // We cannot statically verify inclusion. The user will get a
    // linker/runtime error if LOG_DEBUGF etc. are undefined.
#endif

namespace wasapi {

/// Wire wasapi internal logging to core::Logger.
///
/// Requires:
///   1. #include <core/logger.hpp> before this header.
///   2. core::Logger::instance().initialize(...) has been called.
///
/// After this call, all wasapi messages are routed to core::Logger
/// with the "[wasapi] " prefix.
inline void connectCoreLogger() noexcept {
    setLogCallback([](LogLevel level, const char* msg) noexcept {
        try {
            switch (level) {
                case LogLevel::Debug:
                    LOG_DEBUGF("[wasapi] %s", msg);
                    break;
                case LogLevel::Info:
                    LOG_INFOF("[wasapi] %s", msg);
                    break;
                case LogLevel::Warning:
                    LOG_WARNINGF("[wasapi] %s", msg);
                    break;
                case LogLevel::Error:
                    LOG_ERRORF("[wasapi] %s", msg);
                    break;
            }
        } catch (...) {}
    });
}

} // namespace wasapi
