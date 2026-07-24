/***************************************************************************
 * WASAPI - Windows Audio Library
 * 
 * Audio Format Converter Implementation
 * 
 * License: MIT
 * Author: Strg-Alt-Entf-0x00
 * Date: 2026-06-09
 ***************************************************************************/

#include "format_converter.h"
#include <algorithm>
#include <cmath>

namespace wasapi {
namespace internal {

void AudioFormatConverter::int16ToFloat32(const int16_t* src, float* dst, size_t sampleCount) noexcept {
    // Compiler can auto-vectorize this loop (SSE2/AVX)
    for (size_t i = 0; i < sampleCount; ++i) {
        dst[i] = static_cast<float>(src[i]) * kInt16ScaleInv;
    }
}

void AudioFormatConverter::float32ToInt16(const float* src, int16_t* dst, size_t sampleCount) noexcept {
    for (size_t i = 0; i < sampleCount; ++i) {
        // Clamp to [-1.0, 1.0]
        float clamped = clamp(src[i]);
        
        // Scale to Int16 range
        // Note: Int16 range is asymmetric: [-32768, 32767]
        // -1.0 maps to -32768, +1.0 maps to +32767
        float scaled = clamped * (clamped < 0.0f ? 32768.0f : 32767.0f);
        
        // Round to nearest integer
        dst[i] = static_cast<int16_t>(std::round(scaled));
    }
}

void AudioFormatConverter::int24ToFloat32(const int32_t* src, float* dst, size_t sampleCount) noexcept {
    // Windows WASAPI uses left-justified 24-bit (upper 24 bits)
    // Example: 0x00FFFFFF = max positive, 0xFF000000 = max negative
    
    for (size_t i = 0; i < sampleCount; ++i) {
        // Extract 24-bit value from 32-bit container (left-justified)
        int32_t value = src[i] >> 8;  // Shift right to get 24-bit signed value
        
        // Sign-extend from 24-bit to 32-bit
        if (value & 0x00800000) {  // Check sign bit (bit 23)
            value |= 0xFF000000;    // Sign-extend with 1s
        }
        
        // Convert to float
        dst[i] = static_cast<float>(value) * kInt24ScaleInv;
    }
}

void AudioFormatConverter::float32ToInt24(const float* src, int32_t* dst, size_t sampleCount) noexcept {
    for (size_t i = 0; i < sampleCount; ++i) {
        // Clamp to [-1.0, 1.0] and scale to 24-bit range
        float clamped = clamp(src[i]);
        float scaled = clamped * kInt24Scale;
        
        // Round to nearest integer
        int32_t value = static_cast<int32_t>(std::round(scaled));
        
        // Clamp to 24-bit signed range [-8388608, 8388607]
        value = std::clamp(value, -8388608, 8388607);
        
        // Left-justify: shift left by 8 bits
        dst[i] = value << 8;
    }
}

void AudioFormatConverter::int32ToFloat32(const int32_t* src, float* dst, size_t sampleCount) noexcept {
    for (size_t i = 0; i < sampleCount; ++i) {
        dst[i] = static_cast<float>(src[i]) * kInt32ScaleInv;
    }
}

void AudioFormatConverter::float32ToInt32(const float* src, int32_t* dst, size_t sampleCount) noexcept {
    for (size_t i = 0; i < sampleCount; ++i) {
        // Clamp to [-1.0, 1.0] and scale
        float clamped = clamp(src[i]);
        double scaled = static_cast<double>(clamped) * kInt32Scale;  // Use double for precision
        
        // Round to nearest integer
        dst[i] = static_cast<int32_t>(std::round(scaled));
    }
}

} // namespace internal
} // namespace wasapi

