/***************************************************************************
 * WASAPI - Windows Audio Library
 * 
 * Loopback Recording Example
 * 
 * Demonstrates:
 * - Loopback capture ("What You Hear")
 * - System audio recording
 * - AUDCLNT_STREAMFLAGS_LOOPBACK usage
 * 
 * License: MIT
 * Author: Strg-Alt-Entf-0x00
 * Date: 2026-06-09
 ***************************************************************************/

#include <wasapi/device.h>
#include <wasapi/device_enumerator.h>
#include <iostream>
#include <vector>
#include <iomanip>
#include <cmath>
#include <algorithm>

using namespace wasapi;

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << "    WASAPI Loopback Recording Example" << std::endl;
    std::cout << "    (\"What You Hear\" System Audio Capture)" << std::endl;
    std::cout << "==================================================" << std::endl;
    
    try {
        // Get default playback device (for loopback)
        DeviceEnumerator enumerator;
        auto defaultPlayback = enumerator.getDefaultDevice(DeviceType::Playback);
        
        std::wcout << L"\nCapturing from: " << defaultPlayback.name << std::endl;
        std::cout << "(All system audio playing through this device)" << std::endl;
        
        // Configure loopback device
        DeviceConfig config;
        config.deviceId = L"default";
        config.sampleRateHz = 48000;
        config.channelCount = 2;  // Stereo
        config.format = AudioFormat::Float32;
        config.shareMode = ShareMode::Shared;
        config.bufferSizeMs = 10;
        config.deviceType = DeviceType::Loopback;  // Loopback capture
        
        std::cout << "\nConfiguration:" << std::endl;
        std::cout << "  Sample Rate: " << config.sampleRateHz << " Hz" << std::endl;
        std::cout << "  Channels: " << config.channelCount << " (Stereo)" << std::endl;
        std::cout << "  Format: Float32" << std::endl;
        std::cout << "  Buffer Size: " << config.bufferSizeMs << " ms" << std::endl;
        std::cout << "  Device Type: Loopback (System Audio)" << std::endl;
        
        // Create loopback device
        std::cout << "\nCreating loopback capture device..." << std::endl;
        auto device = Device::create(config);
        
        // Recording buffer
        std::vector<float> recording;
        size_t frameCount = 0;
        
        // Set capture callback
        device->setCallback([&](float* buffer, uint32_t frames) {
            // Append captured system audio to recording buffer
            size_t sampleCount = frames * config.channelCount;
            recording.insert(recording.end(), buffer, buffer + sampleCount);
            frameCount += frames;
        });
        
        // Start recording
        std::cout << "Starting loopback recording..." << std::endl;
        device->start();
        
        std::cout << "\n🔊 RECORDING SYSTEM AUDIO..." << std::endl;
        std::cout << "Play any audio (music, videos, etc.) and it will be captured." << std::endl;
        std::cout << "Press Enter to stop recording." << std::endl;
        std::cin.get();
        
        // Stop recording
        device->stop();
        
        // Show statistics
        std::cout << "\n==================================================" << std::endl;
        std::cout << "Recording Statistics:" << std::endl;
        std::cout << "==================================================" << std::endl;
        std::cout << "  Samples Recorded: " << recording.size() << std::endl;
        std::cout << "  Frames Recorded: " << frameCount << std::endl;
        std::cout << "  Duration: " << std::fixed << std::setprecision(2)
                  << (static_cast<double>(frameCount) / config.sampleRateHz) 
                  << " seconds" << std::endl;
        
        // Calculate peak amplitude (per channel)
        float peakL = 0.0f;
        float peakR = 0.0f;
        for (size_t i = 0; i < recording.size(); i += 2) {
            float absL = std::abs(recording[i]);
            if (absL > peakL) peakL = absL;
            
            if (i + 1 < recording.size()) {
                float absR = std::abs(recording[i + 1]);
                if (absR > peakR) peakR = absR;
            }
        }
        
        std::cout << "  Peak Amplitude (L): " << std::fixed << std::setprecision(4) 
                  << peakL << " (" << (peakL * 100.0f) << "%)" << std::endl;
        std::cout << "  Peak Amplitude (R): " << std::fixed << std::setprecision(4) 
                  << peakR << " (" << (peakR * 100.0f) << "%)" << std::endl;
        
        // Calculate RMS (loudness)
        double sumSquares = 0.0;
        for (float sample : recording) {
            sumSquares += static_cast<double>(sample) * static_cast<double>(sample);
        }
        float rms = static_cast<float>(std::sqrt(sumSquares / recording.size()));
        
        std::cout << "  RMS Level: " << std::fixed << std::setprecision(4)
                  << rms << " (" << (rms * 100.0f) << "%)" << std::endl;
        
        if (rms < 0.001f) {
            std::cout << "\n⚠️  Warning: Very low audio level detected." << std::endl;
            std::cout << "    Make sure audio was playing during recording." << std::endl;
        }
        
        std::cout << "\n✅ Loopback recording completed successfully!" << std::endl;
        std::cout << "\nUse Cases:" << std::endl;
        std::cout << "  - Record streaming audio/video" << std::endl;
        std::cout << "  - Capture game audio" << std::endl;
        std::cout << "  - Record system notifications" << std::endl;
        std::cout << "  - Audio monitoring/analysis" << std::endl;
        
    } catch (const WasapiException& e) {
        std::cout << "\n❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

