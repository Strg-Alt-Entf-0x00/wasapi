#pragma once
/***************************************************************************
 * WASAPI - Windows Audio Library
 *
 * Public Logging Interface
 *
 * By default wasapi emits NO log output (zero overhead).
 * To receive log messages, register a callback via setLogCallback().
 *
 * For core::Logger integration (logger library),
 * include wasapi/logger_adapter.h after core/logger.hpp and call
 * wasapi::connectCoreLogger() once at startup.
 *
 * Thread-Safety:
 *   setLogCallback() - call from main thread before creating wasapi objects.
 *   The callback itself is invoked from whichever thread triggered the log
 *   message. The callback MUST be thread-safe.
 *   The callback is NEVER called from the real-time audio thread.
 *
 * License: MIT
 * Author: Strg-Alt-Entf-0x00
 ***************************************************************************/

#include <functional>
#include <cstdarg>

namespace wasapi {

/// Log severity levels (mirrors core::LogLevel for easy adapter mapping).
enum class LogLevel {
    Debug   = 0,
    Info    = 1,
    Warning = 2,
    Error   = 3
};

/// Signature of the log callback registered by the application.
/// @param level  Severity of the message.
/// @param msg    Null-terminated, formatted message string.
using LogCallback = std::function<void(LogLevel level, const char* msg)>;

/// Register a global log callback for all wasapi library messages.
/// Pass nullptr (or a default-constructed LogCallback) to disable logging.
///
/// @param cb  Callback function (thread-safe, no blocking allowed).
///
/// Example:
/// @code
/// wasapi::setLogCallback([](wasapi::LogLevel lvl, const char* msg) {
///     if (lvl >= wasapi::LogLevel::Warning) {
///         fprintf(stderr, "[wasapi] %s\n", msg);
///     }
/// });
/// @endcode
void setLogCallback(LogCallback cb) noexcept;

} // namespace wasapi
