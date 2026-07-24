#pragma once
/***************************************************************************
 * WASAPI - Windows Audio Library
 *
 * Internal Logging Macros
 *
 * INTERNAL HEADER - Do NOT include from public headers.
 * Include from .cpp files only.
 *
 * Maps to wasapi::internal::log() which checks the registered callback.
 * Zero overhead when no callback is registered (atomic check).
 *
 * License: MIT
 ***************************************************************************/

#include <wasapi/logger.h>

namespace wasapi {
namespace internal {

/// Internal log dispatch - checks registered callback and formats message.
/// Never called from the real-time audio thread.
void log(LogLevel level, const char* fmt, ...) noexcept;

} // namespace internal
} // namespace wasapi

// ---------------------------------------------------------------------------
// Convenience macros (prefer over calling internal::log directly)
// ---------------------------------------------------------------------------

#define WASAPI_LOG_DEBUG(fmt, ...)   ::wasapi::internal::log(::wasapi::LogLevel::Debug,   fmt, ##__VA_ARGS__)
#define WASAPI_LOG_INFO(fmt, ...)    ::wasapi::internal::log(::wasapi::LogLevel::Info,    fmt, ##__VA_ARGS__)
#define WASAPI_LOG_WARNING(fmt, ...) ::wasapi::internal::log(::wasapi::LogLevel::Warning, fmt, ##__VA_ARGS__)
#define WASAPI_LOG_ERROR(fmt, ...)   ::wasapi::internal::log(::wasapi::LogLevel::Error,   fmt, ##__VA_ARGS__)
