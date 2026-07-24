/***************************************************************************
 * WASAPI - Windows Audio Library
 * 
 * Format Negotiator Implementation
 * 
 * License: MIT
 * Author: Strg-Alt-Entf-0x00
 * Date: 2026-06-09
 ***************************************************************************/

#include "format_negotiator.h"
#include <cstring>

namespace wasapi {
namespace internal {

FormatNegotiator::NegotiationResult FormatNegotiator::negotiate(
    ComPtr<IAudioClient3> audioClient,
    const DeviceConfig& config,
    AUDCLNT_SHAREMODE shareMode
) {
    NegotiationResult result;
    result.requestedFormat = config.format;
    result.needsConversion = false;
    
    // Strategy 1: Try requested format exactly
    WAVEFORMATEXTENSIBLE requestedWaveFormat = createFormat(config);
    if (tryFormat(audioClient, requestedWaveFormat, shareMode)) {
        result.format = requestedWaveFormat;
        result.negotiatedFormat = config.format;
        result.negotiatedSampleRate = config.sampleRateHz;
        result.negotiatedChannels = config.channelCount;
        result.needsConversion = false;
        return result;
    }
    
    // Strategy 2: Try Float32 fallback (most compatible)
    if (config.format != AudioFormat::Float32) {
        WAVEFORMATEXTENSIBLE float32Format = createFloat32Format(
            config.sampleRateHz, 
            config.channelCount
        );
        
        if (tryFormat(audioClient, float32Format, shareMode)) {
            result.format = float32Format;
            result.negotiatedFormat = AudioFormat::Float32;
            result.negotiatedSampleRate = config.sampleRateHz;
            result.negotiatedChannels = config.channelCount;
            result.needsConversion = true;
            return result;
        }
    }
    
    // Strategy 3: Try Int16 fallback (universal compatibility)
    if (config.format != AudioFormat::Int16) {
        WAVEFORMATEXTENSIBLE int16Format = createInt16Format(
            config.sampleRateHz,
            config.channelCount
        );
        
        if (tryFormat(audioClient, int16Format, shareMode)) {
            result.format = int16Format;
            result.negotiatedFormat = AudioFormat::Int16;
            result.negotiatedSampleRate = config.sampleRateHz;
            result.negotiatedChannels = config.channelCount;
            result.needsConversion = true;
            return result;
        }
    }
    
    // Strategy 4: Use system mix format (always works in shared mode)
    if (shareMode == AUDCLNT_SHAREMODE_SHARED) {
        result.format = getMixFormat(audioClient);
        result.negotiatedFormat = waveFormatToAudioFormat(&result.format.Format);
        result.negotiatedSampleRate = result.format.Format.nSamplesPerSec;
        result.negotiatedChannels = result.format.Format.nChannels;
        result.needsConversion = (result.negotiatedFormat != config.format ||
                                  result.negotiatedSampleRate != config.sampleRateHz ||
                                  result.negotiatedChannels != config.channelCount);
        return result;
    }
    
    // Failed to find compatible format
    throw WasapiException(
        ErrorCode::InvalidFormat,
        "Failed to negotiate compatible audio format"
    );
}

bool FormatNegotiator::isFormatSupported(
    ComPtr<IAudioClient3> audioClient,
    const WAVEFORMATEX* format,
    AUDCLNT_SHAREMODE shareMode
) noexcept {
    if (!audioClient || !format) {
        return false;
    }
    
    WAVEFORMATEX* closestMatch = nullptr;
    HRESULT hr = audioClient->IsFormatSupported(
        shareMode,
        format,
        &closestMatch
    );
    
    if (closestMatch) {
        CoTaskMemFree(closestMatch);
    }
    
    return SUCCEEDED(hr) && (hr == S_OK);
}

WAVEFORMATEXTENSIBLE FormatNegotiator::getMixFormat(
    ComPtr<IAudioClient3> audioClient
) {
    WAVEFORMATEX* mixFormat = nullptr;
    HRESULT hr = audioClient->GetMixFormat(&mixFormat);
    WASAPI_CHECK_HR(hr, "Failed to get mix format");
    
    WAVEFORMATEXTENSIBLE result = {};
    
    if (mixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        // Already WAVEFORMATEXTENSIBLE
        std::memcpy(&result, mixFormat, sizeof(WAVEFORMATEXTENSIBLE));
    } else {
        // Convert WAVEFORMATEX to WAVEFORMATEXTENSIBLE
        result.Format = *mixFormat;
        result.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        result.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
        result.Samples.wValidBitsPerSample = mixFormat->wBitsPerSample;
        result.dwChannelMask = getChannelMask(mixFormat->nChannels);
        
        // Determine SubFormat based on original format
        if (mixFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
            result.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
        } else {
            result.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
        }
    }
    
    CoTaskMemFree(mixFormat);
    return result;
}

WAVEFORMATEXTENSIBLE FormatNegotiator::createFormat(
    const DeviceConfig& config
) noexcept {
    WAVEFORMATEXTENSIBLE wfex = {};
    wfex.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wfex.Format.nChannels = config.channelCount;
    wfex.Format.nSamplesPerSec = config.sampleRateHz;
    wfex.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    
    // Set format-specific parameters
    switch (config.format) {
        case AudioFormat::Float32:
            wfex.Format.wBitsPerSample = 32;
            wfex.Samples.wValidBitsPerSample = 32;
            wfex.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
            break;
            
        case AudioFormat::Int32:
            wfex.Format.wBitsPerSample = 32;
            wfex.Samples.wValidBitsPerSample = 32;
            wfex.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
            break;
            
        case AudioFormat::Int24:
            wfex.Format.wBitsPerSample = 32;  // 24-bit in 32-bit container
            wfex.Samples.wValidBitsPerSample = 24;
            wfex.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
            break;
            
        case AudioFormat::Int16:
            wfex.Format.wBitsPerSample = 16;
            wfex.Samples.wValidBitsPerSample = 16;
            wfex.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
            break;
    }
    
    wfex.Format.nBlockAlign = (wfex.Format.wBitsPerSample / 8) * wfex.Format.nChannels;
    wfex.Format.nAvgBytesPerSec = wfex.Format.nSamplesPerSec * wfex.Format.nBlockAlign;
    wfex.dwChannelMask = getChannelMask(config.channelCount);
    
    return wfex;
}

AudioFormat FormatNegotiator::waveFormatToAudioFormat(
    const WAVEFORMATEX* format
) noexcept {
    if (!format) {
        return AudioFormat::Float32;  // Default fallback
    }
    
    if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        const WAVEFORMATEXTENSIBLE* wfex = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format);
        
        if (wfex->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
            return AudioFormat::Float32;
        } else if (wfex->SubFormat == KSDATAFORMAT_SUBTYPE_PCM) {
            if (wfex->Format.wBitsPerSample == 16) {
                return AudioFormat::Int16;
            } else if (wfex->Format.wBitsPerSample == 32) {
                if (wfex->Samples.wValidBitsPerSample == 24) {
                    return AudioFormat::Int24;
                } else {
                    return AudioFormat::Int32;
                }
            }
        }
    } else if (format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        return AudioFormat::Float32;
    } else if (format->wFormatTag == WAVE_FORMAT_PCM) {
        if (format->wBitsPerSample == 16) {
            return AudioFormat::Int16;
        } else if (format->wBitsPerSample == 32) {
            return AudioFormat::Int32;
        }
    }
    
    return AudioFormat::Float32;  // Default fallback
}

DWORD FormatNegotiator::getChannelMask(uint16_t channelCount) noexcept {
    switch (channelCount) {
        case 1:  // Mono
            return SPEAKER_FRONT_CENTER;
            
        case 2:  // Stereo
            return SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
            
        case 3:  // 2.1
            return SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_LOW_FREQUENCY;
            
        case 4:  // Quad
            return SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT |
                   SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT;
            
        case 5:  // 4.1
            return SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT |
                   SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT |
                   SPEAKER_LOW_FREQUENCY;
            
        case 6:  // 5.1 Surround
            return SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT |
                   SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY |
                   SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT;
            
        case 7:  // 6.1
            return SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT |
                   SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY |
                   SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT |
                   SPEAKER_BACK_CENTER;
            
        case 8:  // 7.1 Surround
            return SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT |
                   SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY |
                   SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT |
                   SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT;
            
        default:
            // Fallback: use all channels
            return (1 << channelCount) - 1;
    }
}

bool FormatNegotiator::tryFormat(
    ComPtr<IAudioClient3> audioClient,
    const WAVEFORMATEXTENSIBLE& format,
    AUDCLNT_SHAREMODE shareMode
) noexcept {
    return isFormatSupported(audioClient, &format.Format, shareMode);
}

WAVEFORMATEXTENSIBLE FormatNegotiator::createFloat32Format(
    uint32_t sampleRate,
    uint16_t channelCount
) noexcept {
    WAVEFORMATEXTENSIBLE wfex = {};
    wfex.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wfex.Format.nChannels = channelCount;
    wfex.Format.nSamplesPerSec = sampleRate;
    wfex.Format.wBitsPerSample = 32;
    wfex.Format.nBlockAlign = 4 * channelCount;
    wfex.Format.nAvgBytesPerSec = sampleRate * wfex.Format.nBlockAlign;
    wfex.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    wfex.Samples.wValidBitsPerSample = 32;
    wfex.dwChannelMask = getChannelMask(channelCount);
    wfex.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
    
    return wfex;
}

WAVEFORMATEXTENSIBLE FormatNegotiator::createInt16Format(
    uint32_t sampleRate,
    uint16_t channelCount
) noexcept {
    WAVEFORMATEXTENSIBLE wfex = {};
    wfex.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wfex.Format.nChannels = channelCount;
    wfex.Format.nSamplesPerSec = sampleRate;
    wfex.Format.wBitsPerSample = 16;
    wfex.Format.nBlockAlign = 2 * channelCount;
    wfex.Format.nAvgBytesPerSec = sampleRate * wfex.Format.nBlockAlign;
    wfex.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    wfex.Samples.wValidBitsPerSample = 16;
    wfex.dwChannelMask = getChannelMask(channelCount);
    wfex.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
    
    return wfex;
}

} // namespace internal
} // namespace wasapi

