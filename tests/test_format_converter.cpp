/***************************************************************************
 * WASAPI - Windows Audio Library
 * 
 * Format Converter Test Program
 * 
 * Tests all audio format conversions for correctness and performance.
 * 
 * License: MIT
 * Author: Strg-Alt-Entf-0x00
 * Date: 2026-06-09
 ***************************************************************************/

#include "../src/format_converter.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <chrono>
#include <cmath>

using namespace wasapi::internal;

// Test helper: Generate test signal
void generateTestSignal(std::vector<float>& buffer, float frequency, int sampleRate) {
    const float omega = 2.0f * 3.14159f * frequency / static_cast<float>(sampleRate);
    for (size_t i = 0; i < buffer.size(); ++i) {
        buffer[i] = std::sin(omega * static_cast<float>(i)) * 0.8f;
    }
}

// Test helper: Calculate RMS error
float calculateRmsError(const float* a, const float* b, size_t count) {
    double sumSquares = 0.0;
    for (size_t i = 0; i < count; ++i) {
        double diff = static_cast<double>(a[i]) - static_cast<double>(b[i]);
        sumSquares += diff * diff;
    }
    return static_cast<float>(std::sqrt(sumSquares / static_cast<double>(count)));
}

// Test Int16 ↔ Float32
bool testInt16Conversion() {
    std::cout << "\n=== Testing Int16 ↔ Float32 ===" << std::endl;
    
    const size_t sampleCount = 48000;  // 1 second at 48kHz
    std::vector<float> original(sampleCount);
    std::vector<int16_t> int16(sampleCount);
    std::vector<float> reconstructed(sampleCount);
    
    // Generate 440 Hz test signal
    generateTestSignal(original, 440.0f, 48000);
    
    // Convert: Float32 → Int16 → Float32
    auto start = std::chrono::high_resolution_clock::now();
    AudioFormatConverter::float32ToInt16(original.data(), int16.data(), sampleCount);
    auto mid = std::chrono::high_resolution_clock::now();
    AudioFormatConverter::int16ToFloat32(int16.data(), reconstructed.data(), sampleCount);
    auto end = std::chrono::high_resolution_clock::now();
    
    // Calculate RMS error
    float rmsError = calculateRmsError(original.data(), reconstructed.data(), sampleCount);
    
    // Performance
    auto encode_us = std::chrono::duration_cast<std::chrono::microseconds>(mid - start).count();
    auto decode_us = std::chrono::duration_cast<std::chrono::microseconds>(end - mid).count();
    
    std::cout << "  RMS Error:      " << std::fixed << std::setprecision(8) << rmsError << std::endl;
    std::cout << "  Encode Time:    " << encode_us << " µs (" 
              << (encode_us * 1000.0 / sampleCount) << " ns/sample)" << std::endl;
    std::cout << "  Decode Time:    " << decode_us << " µs (" 
              << (decode_us * 1000.0 / sampleCount) << " ns/sample)" << std::endl;
    
    // Int16 quantization error should be around 1/65536 = 0.000015
    const float expectedError = 0.0001f;
    bool passed = (rmsError < expectedError);
    
    std::cout << "  Status:         " << (passed ? "✅ PASS" : "❌ FAIL") << std::endl;
    
    return passed;
}

// Test Int24 ↔ Float32
bool testInt24Conversion() {
    std::cout << "\n=== Testing Int24 ↔ Float32 ===" << std::endl;
    
    const size_t sampleCount = 48000;
    std::vector<float> original(sampleCount);
    std::vector<int32_t> int24(sampleCount);
    std::vector<float> reconstructed(sampleCount);
    
    // Generate 440 Hz test signal
    generateTestSignal(original, 440.0f, 48000);
    
    // Convert: Float32 → Int24 → Float32
    auto start = std::chrono::high_resolution_clock::now();
    AudioFormatConverter::float32ToInt24(original.data(), int24.data(), sampleCount);
    auto mid = std::chrono::high_resolution_clock::now();
    AudioFormatConverter::int24ToFloat32(int24.data(), reconstructed.data(), sampleCount);
    auto end = std::chrono::high_resolution_clock::now();
    
    // Calculate RMS error
    float rmsError = calculateRmsError(original.data(), reconstructed.data(), sampleCount);
    
    // Performance
    auto encode_us = std::chrono::duration_cast<std::chrono::microseconds>(mid - start).count();
    auto decode_us = std::chrono::duration_cast<std::chrono::microseconds>(end - mid).count();
    
    std::cout << "  RMS Error:      " << std::fixed << std::setprecision(8) << rmsError << std::endl;
    std::cout << "  Encode Time:    " << encode_us << " µs (" 
              << (encode_us * 1000.0 / sampleCount) << " ns/sample)" << std::endl;
    std::cout << "  Decode Time:    " << decode_us << " µs (" 
              << (decode_us * 1000.0 / sampleCount) << " ns/sample)" << std::endl;
    
    // Int24 quantization error should be around 1/16777216 = 0.00000006
    const float expectedError = 0.000001f;
    bool passed = (rmsError < expectedError);
    
    std::cout << "  Status:         " << (passed ? "✅ PASS" : "❌ FAIL") << std::endl;
    
    return passed;
}

// Test Int32 ↔ Float32
bool testInt32Conversion() {
    std::cout << "\n=== Testing Int32 ↔ Float32 ===" << std::endl;
    
    const size_t sampleCount = 48000;
    std::vector<float> original(sampleCount);
    std::vector<int32_t> int32(sampleCount);
    std::vector<float> reconstructed(sampleCount);
    
    // Generate 440 Hz test signal
    generateTestSignal(original, 440.0f, 48000);
    
    // Convert: Float32 → Int32 → Float32
    auto start = std::chrono::high_resolution_clock::now();
    AudioFormatConverter::float32ToInt32(original.data(), int32.data(), sampleCount);
    auto mid = std::chrono::high_resolution_clock::now();
    AudioFormatConverter::int32ToFloat32(int32.data(), reconstructed.data(), sampleCount);
    auto end = std::chrono::high_resolution_clock::now();
    
    // Calculate RMS error
    float rmsError = calculateRmsError(original.data(), reconstructed.data(), sampleCount);
    
    // Performance
    auto encode_us = std::chrono::duration_cast<std::chrono::microseconds>(mid - start).count();
    auto decode_us = std::chrono::duration_cast<std::chrono::microseconds>(end - mid).count();
    
    std::cout << "  RMS Error:      " << std::fixed << std::setprecision(8) << rmsError << std::endl;
    std::cout << "  Encode Time:    " << encode_us << " µs (" 
              << (encode_us * 1000.0 / sampleCount) << " ns/sample)" << std::endl;
    std::cout << "  Decode Time:    " << decode_us << " µs (" 
              << (decode_us * 1000.0 / sampleCount) << " ns/sample)" << std::endl;
    
    // Int32 quantization error should be near zero
    const float expectedError = 0.0000001f;
    bool passed = (rmsError < expectedError);
    
    std::cout << "  Status:         " << (passed ? "✅ PASS" : "❌ FAIL") << std::endl;
    
    return passed;
}

// Test edge cases
bool testEdgeCases() {
    std::cout << "\n=== Testing Edge Cases ===" << std::endl;
    
    bool allPassed = true;
    
    // Test 1: Clamping (values > 1.0)
    {
        float input[] = {-1.5f, -1.0f, 0.0f, 1.0f, 1.5f};
        int16_t output[5];
        AudioFormatConverter::float32ToInt16(input, output, 5);
        
        bool passed = (output[0] == -32768 && output[1] == -32768 && 
                      output[2] == 0 && output[3] == 32767 && output[4] == 32767);
        
        std::cout << "  Clamping Test:  " << (passed ? "✅ PASS" : "❌ FAIL") << std::endl;
        allPassed &= passed;
    }
    
    // Test 2: Zero signal
    {
        std::vector<float> zeros(1024, 0.0f);
        std::vector<int16_t> output(1024);
        AudioFormatConverter::float32ToInt16(zeros.data(), output.data(), 1024);
        
        bool passed = true;
        for (int16_t val : output) {
            if (val != 0) {
                passed = false;
                break;
            }
        }
        
        std::cout << "  Zero Signal:    " << (passed ? "✅ PASS" : "❌ FAIL") << std::endl;
        allPassed &= passed;
    }
    
    // Test 3: Max amplitude
    {
        float input[] = {-1.0f, 1.0f};
        int16_t output[2];
        AudioFormatConverter::float32ToInt16(input, output, 2);
        
        bool passed = (output[0] == -32768 && output[1] == 32767);
        
        std::cout << "  Max Amplitude:  " << (passed ? "✅ PASS" : "❌ FAIL") << std::endl;
        allPassed &= passed;
    }
    
    return allPassed;
}

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << "    WASAPI Format Converter Test Suite" << std::endl;
    std::cout << "==================================================" << std::endl;
    
    bool allPassed = true;
    
    allPassed &= testInt16Conversion();
    allPassed &= testInt24Conversion();
    allPassed &= testInt32Conversion();
    allPassed &= testEdgeCases();
    
    std::cout << "\n==================================================" << std::endl;
    if (allPassed) {
        std::cout << "  ✅ ALL TESTS PASSED" << std::endl;
    } else {
        std::cout << "  ❌ SOME TESTS FAILED" << std::endl;
    }
    std::cout << "==================================================" << std::endl;
    
    return allPassed ? 0 : 1;
}

