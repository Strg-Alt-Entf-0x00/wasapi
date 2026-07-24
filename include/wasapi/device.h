#pragma once
/***************************************************************************
 * WASAPI - Windows Audio Library
 *
 * Main Device Interface
 *
 * Features:
 * - IAudioClient3 for low-latency shared mode
 * - Event-driven architecture (no polling)
 * - MMCSS thread priority support
 * - Automatic buffer management
 * - Robust error handling
 * - Peak level metering (IAudioMeterInformation)
 * - Per-channel volume control (IAudioStreamVolume)
 * - Session lifecycle events (IAudioSessionEvents)
 * - Stream position (IAudioClock)
 *
 * License: MIT
 * Author: AI-Projects Team
 * Date: 2026-06-08
 * Updated: 2026-06-19 (v2.0.0)
 ***************************************************************************/

#include "types.h"
#include "error.h"
#include <memory>

namespace wasapi {

/**
 * @brief Audio device: playback, capture, or loopback.
 *
 * Represents a fully initialized WASAPI audio endpoint.
 * Use Device::create() to construct (factory pattern).
 *
 * Lifecycle:
 *   1. Device::create(config)     - Initialize and negotiate format
 *   2. setCallback(callback)      - Provide audio data handler
 *   3. setSessionEventCallback()  - Optionally handle disconnects
 *   4. start()                    - Begin audio streaming
 *   5. stop()                     - Stop streaming
 *   (Device destructor calls stop() + cleanup automatically)
 *
 * Thread-Safety:
 *   - All methods are thread-safe EXCEPT setCallback() and setSessionEventCallback()
 *   - setCallback() / setSessionEventCallback() must be called before start()
 *   - AudioCallback is invoked from the real-time audio thread
 *   - SessionEventCallback is invoked from a COM notification thread
 *
 * Example (Playback):
 * @code
 * wasapi::DeviceConfig config;
 * config.deviceId     = L"default";
 * config.sampleRateHz = 48000;
 * config.channelCount = 2;
 * config.bufferSizeMs = wasapi::toMs(wasapi::LatencyMode::Low);
 *
 * auto device = wasapi::Device::create(config);
 *
 * device->setSessionEventCallback([](wasapi::SessionEvent e) {
 *     if (e == wasapi::SessionEvent::Disconnected) { // handle }
 * });
 *
 * device->setCallback([](float* buf, uint32_t frames) {
 *     // fill buf with audio samples (stereo interleaved float32)
 * });
 *
 * device->start();
 * // ... audio running ...
 * device->stop();
 * @endcode
 */
class Device {
public:
    // -----------------------------------------------------------------------
    // Construction
    // -----------------------------------------------------------------------

    /**
     * @brief Create and initialize an audio device.
     *
     * @param config  Device configuration (validated before use).
     * @return        Initialized Device ready for setCallback() + start().
     * @throws WasapiException if device creation or format negotiation fails.
     */
    [[nodiscard]] static std::unique_ptr<Device> create(const DeviceConfig& config);

    /// Destructor: calls stop() and releases all resources.
    ~Device();

    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&) noexcept;
    Device& operator=(Device&&) noexcept;

    // -----------------------------------------------------------------------
    // Core streaming control
    // -----------------------------------------------------------------------

    /**
     * @brief Start audio streaming.
     *
     * Primes the buffer and starts the real-time audio thread.
     *
     * @return true if started (or already running).
     * @throws WasapiException if callback not set, or IAudioClient::Start() fails.
     *
     * Preconditions:
     *   - setCallback() must have been called.
     *   - Device must not already be running.
     */
    bool start();

    /**
     * @brief Stop audio streaming.
     *
     * Signals the audio thread to stop and waits for it to terminate.
     * Safe to call multiple times or from a different thread.
     */
    void stop();

    /**
     * @brief Fast buffer reset without full reinitialization.
     *
     * Implements the WASAPI pattern: Stop -> Reset -> Prime -> Start.
     * Approximately 10ms vs 100-350ms for full reinit.
     *
     * Use cases: underrun recovery, clearing stale data, quick restart.
     * @throws WasapiException if Reset() fails.
     */
    void reset();

    // -----------------------------------------------------------------------
    // Callbacks
    // -----------------------------------------------------------------------

    /**
     * @brief Set the audio data callback.
     *
     * Must be called before start(). Cannot be changed while running.
     * The callback runs on the real-time audio thread:
     *   - No heap allocation
     *   - No blocking operations
     *   - No exceptions
     *
     * @param callback  Audio processing function.
     * @throws WasapiException if device is currently running.
     */
    void setCallback(AudioCallback callback);

    /**
     * @brief Set a session lifecycle event callback.
     *
     * Notified when the audio session is disconnected, the format changes,
     * or the volume is changed externally by Windows.
     *
     * The most important case: SessionEvent::Disconnected - means the device
     * was unplugged or exclusive-mode was taken. The audio thread will stop
     * silently; handle this event to inform the user or switch devices.
     *
     * Thread-Safety: Must be called before start(). The callback itself
     * is invoked from a COM notification thread - must be thread-safe.
     *
     * @param cb  Callback function (nullptr to clear).
     */
    void setSessionEventCallback(SessionEventCallback cb);

    // -----------------------------------------------------------------------
    // Status and diagnostics
    // -----------------------------------------------------------------------

    /// Returns true if the audio stream is currently active.
    [[nodiscard]] bool isRunning() const noexcept;

    /// Returns approximate current audio latency in milliseconds.
    [[nodiscard]] uint32_t latencyMs() const noexcept;

    /// Returns the device metadata (id, name, type, isDefault).
    [[nodiscard]] DeviceInfo deviceInfo() const;

    /// Returns the effective configuration (may differ from requested due to format negotiation).
    [[nodiscard]] const DeviceConfig& config() const noexcept;

    /// Returns the count of buffer underruns (playback) or overruns (capture) since last reset.
    [[nodiscard]] uint32_t underrunCount() const noexcept;

    // -----------------------------------------------------------------------
    // Volume control (Playback only)
    // -----------------------------------------------------------------------

    /**
     * @brief Set master session volume.
     * @param level  [0.0, 1.0]  (0.0 = silence, 1.0 = full)
     * @throws WasapiException if not available (capture device or exclusive mode).
     */
    void setVolume(float level);

    /// Returns master session volume [0.0, 1.0].
    [[nodiscard]] float getVolume() const;

    /// Mute/unmute the session.
    void setMuted(bool muted);

    /// Returns true if the session is currently muted.
    [[nodiscard]] bool isMuted() const;

    /**
     * @brief Set per-channel volume (independent L/R/surround control).
     *
     * Uses IAudioStreamVolume for fine-grained per-channel attenuation.
     *
     * @param channel  Zero-based channel index (0=Left, 1=Right, etc.)
     * @param level    [0.0, 1.0]
     * @throws WasapiException if channel index out of range or not available.
     */
    void setChannelVolume(uint16_t channel, float level);

    /**
     * @brief Get per-channel volume.
     * @param channel  Zero-based channel index.
     * @return         Volume level [0.0, 1.0].
     * @throws WasapiException if channel index out of range or not available.
     */
    [[nodiscard]] float getChannelVolume(uint16_t channel) const;

    /// Returns the configured number of audio channels.
    [[nodiscard]] uint16_t getChannelCount() const noexcept;

    // -----------------------------------------------------------------------
    // Peak metering
    // -----------------------------------------------------------------------

    /**
     * @brief Get the endpoint peak level since the last call.
     *
     * Uses IAudioMeterInformation. Returns the highest sample magnitude
     * recorded on the endpoint since the previous call.
     *
     * Note: Measures the entire endpoint mix, not just this session.
     *       Only meaningful in Shared mode. Returns 0.0f in Exclusive mode.
     *
     * @return Peak sample value [0.0, 1.0]. Returns 0.0f if not available.
     */
    [[nodiscard]] float getPeakLevel() const noexcept;

    // -----------------------------------------------------------------------
    // Stream position (IAudioClock)
    // -----------------------------------------------------------------------

    /**
     * @brief Get the current stream position.
     *
     * Returns the number of frames played/captured since start(),
     * with a high-resolution QPC timestamp for synchronization.
     *
     * Returns all-zero AudioPosition if the device is not running
     * or the clock is unavailable.
     *
     * Thread-Safety: Thread-safe. May be called from any thread.
     */
    [[nodiscard]] AudioPosition getPosition() const noexcept;

private:
    explicit Device(const DeviceConfig& config);

    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace wasapi
