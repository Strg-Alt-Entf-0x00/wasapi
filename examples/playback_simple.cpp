/***************************************************************************
 * WASAPI Example - Simple Sine Wave Playback
 * 
 * Demonstrates:
 * - Device creation
 * - Audio callback
 * - Low-latency playback
 * 
 * License: MIT
 ***************************************************************************/

#include <wasapi/device.h>
#include <wasapi/device_enumerator.h>
#include <iostream>
#include <cmath>
#include <thread>
#include <chrono>

constexpr float kPi = 3.14159265358979323846f;

int main() {
    try {
        using namespace wasapi;
        
        std::wcout << L"=== WASAPI Simple Playback Example ===\n\n";
        
        // Get default device info
        DeviceEnumerator enumerator;
        auto defaultDevice = enumerator.getDefaultDevice(DeviceType::Playback);
        std::wcout << L"Using device: " << defaultDevice.name << L"\n\n";
        
        // Configure device for low-latency playback
        DeviceConfig config;
        config.deviceId = L"default";
        config.sampleRateHz = 48000;
        config.channelCount = 2;  // Stereo
        config.format = AudioFormat::Float32;
        config.shareMode = ShareMode::Shared;
        config.bufferSizeMs = 10;  // Low latency!
        
        // Create device
        std::cout << "Creating audio device...\n";
        auto device = Device::create(config);
        
        // Set callback - Generate 440 Hz sine wave
        float phase = 0.0f;
        const float frequency = 440.0f;  // A4 note
        const float phaseIncrement = (2.0f * kPi * frequency) / config.sampleRateHz;
        
        device->setCallback([&](float* buffer, uint32_t frameCount) {
            for (uint32_t i = 0; i < frameCount; ++i) {
                float sample = std::sin(phase) * 0.25f;  // 25% volume
                
                buffer[i * 2 + 0] = sample;  // Left channel
                buffer[i * 2 + 1] = sample;  // Right channel
                
                phase += phaseIncrement;
                if (phase >= 2.0f * kPi) {
                    phase -= 2.0f * kPi;
                }
            }
        });
        
        // Start playback
        std::cout << "Starting playback...\n";
        device->start();
        
        std::cout << "\nPlaying 440 Hz tone (A4 note)\n";
        std::cout << "Configuration:\n";
        std::cout << "  Sample Rate: " << config.sampleRateHz << " Hz\n";
        std::cout << "  Channels: " << config.channelCount << "\n";
        std::cout << "  Buffer Size: " << config.bufferSizeMs << " ms\n";
        std::cout << "  Format: Float32\n";
        std::cout << "  Share Mode: " << (config.shareMode == ShareMode::Shared ? "Shared" : "Exclusive") << "\n";
        std::cout << "\nPress Enter to stop...\n";
        
        std::cin.get();
        
        // Stop playback
        std::cout << "\nStopping playback...\n";
        device->stop();
        
        // Statistics
        std::cout << "Statistics:\n";
        std::cout << "  Underruns: " << device->underrunCount() << "\n";
        std::cout << "  Final Latency: " << device->latencyMs() << " ms\n";
        
        std::cout << "\nDone!\n";
        
        return 0;
    }
    catch (const wasapi::WasapiException& e) {
        std::cerr << "WASAPI Error: " << e.what() << "\n";
        std::cerr << "Error Code: " << static_cast<int>(e.code()) << "\n";
        return 1;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
