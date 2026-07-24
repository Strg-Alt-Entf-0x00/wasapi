/***************************************************************************
 * WASAPI - Windows Audio Library
 * 
 * Microphone Recording Example
 * 
 * Demonstrates:
 * - IAudioCaptureClient usage
 * - Real-time microphone recording
 * - Audio buffer management
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

using namespace wasapi;

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << "    WASAPI Microphone Recording Example" << std::endl;
    std::cout << "==================================================" << std::endl;
    
    try {
        // Enumerate capture devices
        DeviceEnumerator enumerator;
        auto captureDevices = enumerator.enumerateDevices(DeviceType::Capture);
        
        if (captureDevices.empty()) {
            std::cout << "\nNo microphones found!" << std::endl;
            return 1;
        }
        
        // Show available microphones
        std::cout << "\nAvailable Microphones:" << std::endl;
        for (size_t i = 0; i < captureDevices.size(); ++i) {
            std::wcout << "  [" << i << "] " << captureDevices[i].name;
            if (captureDevices[i].isDefault) {
                std::wcout << L" (DEFAULT)";
            }
            std::wcout << std::endl;
        }
        
        // Get default microphone
        auto defaultMic = enumerator.getDefaultDevice(DeviceType::Capture);
        std::wcout << L"\nUsing: " << defaultMic.name << std::endl;
        
        // Configure capture device
        DeviceConfig config;
        config.deviceId = L"default";
        config.sampleRateHz = 48000;
        config.channelCount = 1;  // Mono
        config.format = AudioFormat::Float32;
        config.shareMode = ShareMode::Shared;
        config.bufferSizeMs = 10;
        config.deviceType = DeviceType::Capture;  // Microphone recording
        
        std::cout << "\nConfiguration:" << std::endl;
        std::cout << "  Sample Rate: " << config.sampleRateHz << " Hz" << std::endl;
        std::cout << "  Channels: " << config.channelCount << " (Mono)" << std::endl;
        std::cout << "  Format: Float32" << std::endl;
        std::cout << "  Buffer Size: " << config.bufferSizeMs << " ms" << std::endl;
        std::cout << "  Device Type: Microphone" << std::endl;
        
        // Create capture device
        std::cout << "\nCreating capture device..." << std::endl;
        auto device = Device::create(config);
        
        // Recording buffer
        std::vector<float> recording;
        size_t frameCount = 0;
        
        // Set capture callback
        device->setCallback([&](float* buffer, uint32_t frames) {
            // Append captured audio to recording buffer
            recording.insert(recording.end(), buffer, buffer + frames);
            frameCount += frames;
        });
        
        // Start recording
        std::cout << "Starting recording..." << std::endl;
        device->start();
        
        std::cout << "\n🎤 RECORDING... Speak into your microphone!" << std::endl;
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
                  << (static_cast<double>(recording.size()) / config.sampleRateHz) 
                  << " seconds" << std::endl;
        
        // Calculate peak amplitude
        float peak = 0.0f;
        for (float sample : recording) {
            float abs_sample = std::abs(sample);
            if (abs_sample > peak) {
                peak = abs_sample;
            }
        }
        
        std::cout << "  Peak Amplitude: " << std::fixed << std::setprecision(4) 
                  << peak << " (" << (peak * 100.0f) << "%)" << std::endl;
        
        // Calculate RMS (loudness)
        double sumSquares = 0.0;
        for (float sample : recording) {
            sumSquares += static_cast<double>(sample) * static_cast<double>(sample);
        }
        float rms = static_cast<float>(std::sqrt(sumSquares / recording.size()));
        
        std::cout << "  RMS Level: " << std::fixed << std::setprecision(4)
                  << rms << " (" << (rms * 100.0f) << "%)" << std::endl;
        
        std::cout << "\n✅ Recording completed successfully!" << std::endl;
        std::cout << "\nNote: To save to file, integrate libsndfile:" << std::endl;
        std::cout << "  sf_open(\"recording.wav\", SFM_WRITE, &sfinfo);" << std::endl;
        std::cout << "  sf_writef_float(file, recording.data(), frameCount);" << std::endl;
        
    } catch (const WasapiException& e) {
        std::cout << "\n❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

