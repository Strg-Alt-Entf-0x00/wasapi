#pragma once
/***************************************************************************
 * WASAPI - Windows Audio Library
 *
 * Hot-Plug / Device Change Notifications
 *
 * Monitors audio endpoints for plug/unplug events, default device changes,
 * and state changes. Uses Windows IMMNotificationClient internally.
 *
 * Usage:
 * @code
 * wasapi::DeviceWatcher watcher([](const wasapi::DeviceChangeInfo& info) {
 *     switch (info.event) {
 *         case wasapi::DeviceEvent::Added:
 *             // New device plugged in
 *             break;
 *         case wasapi::DeviceEvent::Removed:
 *             // Device unplugged - stop streams using this device!
 *             break;
 *         case wasapi::DeviceEvent::DefaultChanged:
 *             // Windows default device changed
 *             break;
 *         case wasapi::DeviceEvent::StateChanged:
 *             // Device enabled/disabled
 *             break;
 *     }
 * });
 *
 * // Watching starts automatically in constructor.
 * // Destructor stops watching and cleans up.
 * @endcode
 *
 * Thread-Safety:
 *   The callback is invoked from a Windows COM notification thread.
 *   The callback MUST be thread-safe and MUST NOT block.
 *
 * License: MIT
 * Author: AI-Projects Team
 * Date: 2026-06-19
 ***************************************************************************/

#include "types.h"
#include "error.h"
#include <memory>
#include <functional>

namespace wasapi {

/**
 * @brief Device hot-plug event types.
 */
enum class DeviceEvent {
    Added,          ///< A new audio endpoint was connected
    Removed,        ///< An audio endpoint was disconnected
    DefaultChanged, ///< The Windows default audio device changed
    StateChanged    ///< An endpoint's state changed (enabled/disabled/unplugged)
};

/**
 * @brief Information about a device change event.
 */
struct DeviceChangeInfo {
    std::wstring deviceId;       ///< Endpoint ID string of the affected device
    DeviceEvent  event;          ///< Type of event
    DeviceType   affectedType;   ///< Playback or Capture (best-effort; Unknown if indeterminate)
    bool         isDefault;      ///< True for DefaultChanged events
};

/// Callback signature for device change notifications.
/// Thread-Safety: Called from COM notification thread - must be thread-safe.
/// Must NOT block, throw, or call back into wasapi synchronously.
using DeviceChangeCallback = std::function<void(const DeviceChangeInfo& info)>;

/**
 * @brief Audio device hot-plug watcher.
 *
 * Monitors all audio endpoints for plug/unplug and default-device changes.
 * Watching begins in the constructor and stops in the destructor (RAII).
 *
 * Thread-Safety: All public methods are thread-safe.
 */
class DeviceWatcher {
public:
    /**
     * @brief Construct and start watching.
     *
     * @param callback  Function called on any device change event.
     *                  See DeviceChangeCallback for thread-safety requirements.
     * @throws WasapiException if COM initialization or registration fails.
     */
    explicit DeviceWatcher(DeviceChangeCallback callback);

    /// Destructor: stops watching and releases COM resources.
    ~DeviceWatcher();

    // Non-copyable
    DeviceWatcher(const DeviceWatcher&) = delete;
    DeviceWatcher& operator=(const DeviceWatcher&) = delete;

    // Movable
    DeviceWatcher(DeviceWatcher&&) noexcept;
    DeviceWatcher& operator=(DeviceWatcher&&) noexcept;

    /**
     * @brief Stop watching device changes.
     *
     * Safe to call multiple times.
     * After stop(), no more callbacks will be delivered.
     */
    void stop() noexcept;

    /**
     * @brief Resume watching after stop().
     *
     * @throws WasapiException if re-registration fails.
     */
    void start();

    /// Returns true if the watcher is currently active.
    [[nodiscard]] bool isWatching() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace wasapi
