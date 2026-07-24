#pragma once
/***************************************************************************
 * WASAPI - Windows Audio Library
 * 
 * Format Negotiator
 * 
 * Intelligent audio format negotiation with automatic fallback.
 * Inspired by VLC's WASAPI implementation.
 * 
 * Strategy:
 * 1. Try requested format exactly
 * 2. Try Float32 (most widely supported)
 * 3. Try Int16 (universal compatibility)
 * 4. Try system mix format (always works)
 * 5. Enumerate all supported formats
 * 
 * Thread-Safety: All functions are thread-safe
 * 
 * License: MIT
 * Author: Strg-Alt-Entf-0x00
 * Date: 2026-06-09
 ***************************************************************************/

#include <wasapi/types.h>
#include <wasapi/error.h>
#include <audioclient.h>
#include <wrl/client.h>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace wasapi {
namespace internal {

/**
 * @brief Audio format negotiator
 * 
 * Negotiates the best audio format with WASAPI device.
 * Uses intelligent fallback strategy to maximize compatibility.
 * 
 * Thread-Safety: All methods are thread-safe (no mutable state)
 */
class FormatNegotiator {
public:
    /**
     * @brief Format negotiation result
     */
    struct NegotiationResult {
        WAVEFORMATEXTENSIBLE format;        ///< Negotiated format
        bool needsConversion;               ///< True if format conversion needed
        AudioFormat requestedFormat;        ///< Originally requested format
        AudioFormat negotiatedFormat;       ///< Actually negotiated format
        uint32_t negotiatedSampleRate;      ///< Actual sample rate
        uint16_t negotiatedChannels;        ///< Actual channel count
    };
    
    /**
     * @brief Negotiate audio format with device
     * 
     * Tries multiple strategies to find a working format:
     * 1. Requested format (exact match)
     * 2. Float32 fallback (most compatible)
     * 3. Int16 fallback (universal)
     * 4. System mix format (always supported)
     * 
     * @param audioClient Audio client to negotiate with
     * @param config Desired device configuration
     * @param shareMode Share mode (Shared/Exclusive)
     * @return NegotiationResult Negotiated format details
     * @throws WasapiException if no compatible format found
     * 
     * Example:
     * @code
     * FormatNegotiator negotiator;
     * auto result = negotiator.negotiate(audioClient, config, shareMode);
     * 
     * if (result.needsConversion) {
     *     // Setup format converter
     * }
     * @endcode
     */
    [[nodiscard]] static NegotiationResult negotiate(
        ComPtr<IAudioClient3> audioClient,
        const DeviceConfig& config,
        AUDCLNT_SHAREMODE shareMode
    );
    
    /**
     * @brief Check if specific format is supported
     * 
     * @param audioClient Audio client to check
     * @param format WAVEFORMATEX to test
     * @param shareMode Share mode
     * @return true if format is supported
     */
    [[nodiscard]] static bool isFormatSupported(
        ComPtr<IAudioClient3> audioClient,
        const WAVEFORMATEX* format,
        AUDCLNT_SHAREMODE shareMode
    ) noexcept;
    
    /**
     * @brief Get system mix format
     * 
     * Returns the format Windows is using for audio mixing.
     * This format is always supported in shared mode.
     * 
     * @param audioClient Audio client
     * @return WAVEFORMATEXTENSIBLE System mix format
     * @throws WasapiException if query fails
     */
    [[nodiscard]] static WAVEFORMATEXTENSIBLE getMixFormat(
        ComPtr<IAudioClient3> audioClient
    );
    
    /**
     * @brief Create WAVEFORMATEXTENSIBLE from config
     * 
     * @param config Device configuration
     * @return WAVEFORMATEXTENSIBLE Constructed format
     */
    [[nodiscard]] static WAVEFORMATEXTENSIBLE createFormat(
        const DeviceConfig& config
    ) noexcept;
    
    /**
     * @brief Convert WAVEFORMATEX to AudioFormat enum
     * 
     * @param format WAVEFORMATEX structure
     * @return AudioFormat Enum representation
     */
    [[nodiscard]] static AudioFormat waveFormatToAudioFormat(
        const WAVEFORMATEX* format
    ) noexcept;
    
    /**
     * @brief Get channel mask for channel count
     * 
     * Returns standard Windows channel mask for given channel count.
     * 
     * @param channelCount Number of channels (1-8)
     * @return DWORD Channel mask
     * 
     * Examples:
     * - 1 channel:  SPEAKER_FRONT_CENTER
     * - 2 channels: SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT
     * - 6 channels: 5.1 surround (FL, FR, FC, LFE, BL, BR)
     * - 8 channels: 7.1 surround
     */
    [[nodiscard]] static DWORD getChannelMask(uint16_t channelCount) noexcept;
    
private:
    /**
     * @brief Try to negotiate specific format
     */
    [[nodiscard]] static bool tryFormat(
        ComPtr<IAudioClient3> audioClient,
        const WAVEFORMATEXTENSIBLE& format,
        AUDCLNT_SHAREMODE shareMode
    ) noexcept;
    
    /**
     * @brief Create Float32 format variant
     */
    [[nodiscard]] static WAVEFORMATEXTENSIBLE createFloat32Format(
        uint32_t sampleRate,
        uint16_t channelCount
    ) noexcept;
    
    /**
     * @brief Create Int16 format variant
     */
    [[nodiscard]] static WAVEFORMATEXTENSIBLE createInt16Format(
        uint32_t sampleRate,
        uint16_t channelCount
    ) noexcept;
};

} // namespace internal
} // namespace wasapi

