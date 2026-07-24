/***************************************************************************
 * WASAPI - Windows Audio Library
 *
 * Logger Implementation
 *
 * License: MIT
 * Author: AI-Projects Team
 ***************************************************************************/

#include "wasapi_log.h"

#include <mutex>
#include <atomic>
#include <cstdio>
#include <cstdarg>

namespace wasapi {

// ---------------------------------------------------------------------------
// Internal state (translation-unit local)
// ---------------------------------------------------------------------------

namespace {

std::mutex       g_log_mutex;
LogCallback      g_log_callback;         // protected by g_log_mutex
std::atomic<bool> g_has_callback{false}; // fast-path flag (no lock needed to read)

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void setLogCallback(LogCallback cb) noexcept {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    g_log_callback = std::move(cb);
    g_has_callback.store(static_cast<bool>(g_log_callback), std::memory_order_release);
}

// ---------------------------------------------------------------------------
// Internal dispatch (called by WASAPI_LOG_* macros)
// ---------------------------------------------------------------------------

namespace internal {

void log(LogLevel level, const char* fmt, ...) noexcept {
    // Fast-path: skip entirely if no callback registered
    if (!g_has_callback.load(std::memory_order_acquire)) {
        return;
    }

    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    buf[sizeof(buf) - 1] = '\0'; // guarantee null termination

    std::lock_guard<std::mutex> lock(g_log_mutex);
    if (g_log_callback) {
        try {
            g_log_callback(level, buf);
        } catch (...) {
            // Swallow exceptions from user callback - must never escape
        }
    }
}

} // namespace internal
} // namespace wasapi
