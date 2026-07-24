#pragma once
/***************************************************************************
 * WASAPI - Windows Audio Library
 *
 * Core Type Definitions
 *
 * License: MIT
 * Author: AI-Projects Team
 * Date: 2026-06-08
 * Updated: 2026-06-19 (v2.0.0 - LatencyMode, AudioPosition, SessionEvent)
 ***************************************************************************/

#include <cstdint>
#include <string>
#include <functional>

namespace wasapi {

/**
 * @brief Audio device type
 *
 * Defines the role of the audio device:
 * - Playback: Speaker/headphone output
 * - Capture:  Microphone/line-in input
 * - Loopback: System audio capture ("What You Hear")
 */
enum class DeviceType {
    Playback,
    Capture,
    Loopback
};

/**
 * @brief Audio sharing mode
 *
 * - Shared:    Multiple applications share the device.
 *              Typical latency: 10-15ms with IAudioClient3
 * - Exclusive: Application has exclusive access.
 *              Typical latency: <10ms, requires matching hardware format.
 */
enum class ShareMode {
    Shared,
    Exclusive
};

/**
 * @brief Audio sample format
 *
 * All formats use native endianness (little-endian on x86/x64).
 */
enum class AudioFormat {
    Float32,    ///< 32-bit IEEE floating point [-1.0, 1.0]
    Int16,      ///< 16-bit signed integer PCM
    Int24,      ///< 24-bit signed integer PCM (packed in 32-bit container)
    Int32       ///< 32-bit signed integer PCM
};

/**
 * @brief Predefined latency targets
 *
 * Use with DeviceConfig::bufferSizeMs or the toMs() helper.
 *
 * Example:
 * @code
 * DeviceConfig config;
 * config.bufferSizeMs = wasapi::toMs(wasapi::LatencyMode::Normal); // 20ms
 * @endcode
 */
enum class LatencyMode : uint32_t {
    UltraLow = 3,   ///< 3ms  - Exclusive mode only; highest CPU priority
    Low      = 10,  ///< 10ms - IAudioClient3 Shared; recommended default
    Normal   = 20,  ///< 20ms - Typical for ASR / voice recording
    Buffered = 50,  ///< 50ms - Non-latency-critical playback/capture
    Relaxed  = 100  ///< 100ms - Background capture; lowest CPU usage
};

/**
 * @brief Convert LatencyMode to milliseconds for DeviceConfig::bufferSizeMs.
 * @param mode  Target latency mode.
 * @return      Buffer size in milliseconds.
 */
[[nodiscard]] inline constexpr uint32_t toMs(LatencyMode mode) noexcept {
    return static_cast<uint32_t>(mode);
}

/**
 * @brief Audio stream position snapshot.
 *
 * Returned by Device::getPosition(). All fields are captured atomically
 * at the same point in time from the IAudioClock interface.
 */
struct AudioPosition {
    uint64_t frames;       ///< Frames played/captured since stream start (at config sampleRate)
    uint64_t qpcPosition;  ///< QueryPerformanceCounter timestamp of the snapshot
    double   seconds;      ///< Elapsed stream time in seconds
};

/**
 * @brief Audio session lifecycle events.
 *
 * Delivered via Device::setSessionEventCallback().
 * The callback is invoked from a COM notification thread - must be thread-safe.
 *
 * @note Disconnected is the most critical: audio will stop silently unless handled.
 */
enum class SessionEvent {
    Disconnected,   ///< Device unplugged, exclusive-mode revoked, or session terminated
    FormatChanged,  ///< Windows mix format changed (shared mode)
    VolumeChanged   ///< Session volume or mute state changed externally
};

/// Callback for session lifecycle events.
/// Thread-Safety: Invoked from COM notification thread. Must be thread-safe.
using SessionEventCallback = std::function<void(SessionEvent event)>;

/**
 * @brief Device information
 *
 * Describes an available audio endpoint.
 */
struct DeviceInfo {
    std::wstring id;        ///< Unique device identifier (IMMDevice endpoint ID)
    std::wstring name;      ///< User-friendly device name (e.g. "Speakers (Realtek HD Audio)")
    bool         isDefault; ///< True if this is the Windows default endpoint
    DeviceType   type;      ///< Device type (Playback / Capture / Loopback)
};

/**
 * @brief Device configuration
 *
 * Complete configuration for Device::create().
 * Use isValid() to verify before passing to create().
 */
struct DeviceConfig {
    std::wstring deviceId    = L"default";              ///< Device endpoint ID or L"default"
    uint32_t     sampleRateHz = 48000;                  ///< Sample rate Hz (8000-192000)
    uint16_t     channelCount = 2;                      ///< Channels (1=Mono, 2=Stereo, 6=5.1, 8=7.1)
    AudioFormat  format       = AudioFormat::Float32;   ///< Sample format
    ShareMode    shareMode    = ShareMode::Shared;      ///< Sharing mode
    uint32_t     bufferSizeMs = toMs(LatencyMode::Low); ///< Buffer size ms (use toMs() helper)
    DeviceType   deviceType   = DeviceType::Playback;   ///< Device role

    /**
     * @brief Validates configuration parameters.
     * @return true if all parameters are in valid ranges.
     */
    [[nodiscard]] bool isValid() const noexcept {
        return sampleRateHz  >= 8000  && sampleRateHz  <= 192000
            && channelCount  >= 1     && channelCount  <= 32
            && bufferSizeMs  >= 1     && bufferSizeMs  <= 1000;
    }
};

/**
 * @brief Audio callback function type.
 *
 * Called by the audio engine when:
 * - Playback: buffer space is available (fill with audio data)
 * - Capture:  audio data has been recorded (read from buffer)
 *
 * @param buffer     Pointer to audio buffer (interleaved float32 samples)
 * @param frameCount Number of audio frames (samples per channel)
 *
 * Buffer layout for stereo (2 channels):
 *   buffer[i*2+0] = Left  channel, frame i
 *   buffer[i*2+1] = Right channel, frame i
 *
 * CRITICAL: This callback runs on the real-time audio thread.
 *   - No heap allocation (new, malloc, std::vector resize, etc.)
 *   - No blocking (mutex lock, file I/O, network, sleep)
 *   - No exceptions
 *   - Execution time must be < buffer duration
 */
using AudioCallback = std::function<void(float* buffer, uint32_t frameCount)>;

// ---------------------------------------------------------------------------
// Format helpers
// ---------------------------------------------------------------------------

/// Returns the size of one sample in bytes for the given format.
[[nodiscard]] inline constexpr uint32_t getFormatSizeBytes(AudioFormat format) noexcept {
    switch (format) {
        case AudioFormat::Float32: return 4;
        case AudioFormat::Int16:   return 2;
        case AudioFormat::Int24:   return 4; // Packed in 32-bit container
        case AudioFormat::Int32:   return 4;
        default:                   return 4;
    }
}

/// Returns a human-readable name for the given format.
[[nodiscard]] inline constexpr const char* getFormatName(AudioFormat format) noexcept {
    switch (format) {
        case AudioFormat::Float32: return "Float32";
        case AudioFormat::Int16:   return "Int16";
        case AudioFormat::Int24:   return "Int24";
        case AudioFormat::Int32:   return "Int32";
        default:                   return "Unknown";
    }
}

} // namespace wasapi
