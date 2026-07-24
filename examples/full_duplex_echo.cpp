/***************************************************************************
 * WASAPI - Windows Audio Library
 * 
 * Full-Duplex Echo Test Example
 * 
 * Demonstrates:
 * - Simultaneous playback and capture
 * - Real-time audio routing
 * - Low-latency echo test
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
#include <mutex>
#include <algorithm>

using namespace wasapi;

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << "    WASAPI Full-Duplex Echo Test" << std::endl;
    std::cout << "==================================================" << std::endl;
    
    try {
        // Enumerate devices
        DeviceEnumerator enumerator;
        
        auto defaultMic = enumerator.getDefaultDevice(DeviceType::Capture);
        auto defaultSpeaker = enumerator.getDefaultDevice(DeviceType::Playback);
        
        std::wcout << L"\nMicrophone: " << defaultMic.name << std::endl;
        std::wcout << L"Speaker: " << defaultSpeaker.name << std::endl;
        
        // Configure capture (microphone)
        DeviceConfig captureConfig;
        captureConfig.deviceId = L"default";
        captureConfig.sampleRateHz = 48000;
        captureConfig.channelCount = 1;  // Mono microphone
        captureConfig.format = AudioFormat::Float32;
        captureConfig.bufferSizeMs = 10;
        captureConfig.deviceType = DeviceType::Capture;
        
        // Configure playback (speakers)
        DeviceConfig playbackConfig;
        playbackConfig.deviceId = L"default";
        playbackConfig.sampleRateHz = 48000;
        playbackConfig.channelCount = 2;  // Stereo speakers
        playbackConfig.format = AudioFormat::Float32;
        playbackConfig.bufferSizeMs = 10;
        playbackConfig.deviceType = DeviceType::Playback;
        
        std::cout << "\nConfiguration:" << std::endl;
        std::cout << "  Microphone: 48kHz, Mono" << std::endl;
        std::cout << "  Speakers: 48kHz, Stereo" << std::endl;
        std::cout << "  Buffer Size: 10ms (low latency)" << std::endl;
        
        // Create devices
        std::cout << "\nCreating devices..." << std::endl;
        auto capture = Device::create(captureConfig);
        auto playback = Device::create(playbackConfig);
        
        // Circular buffer for audio routing
        const size_t bufferSize = 48000 * 2;  // 2 seconds max
        std::vector<float> audioBuffer(bufferSize, 0.0f);
        size_t writePos = 0;
        size_t readPos = 0;
        std::mutex bufferMutex;
        
        // Statistics
        size_t framesCapture = 0;
        size_t framesPlayback = 0;
        
        // Capture callback: Write microphone data to buffer
        capture->setCallback([&](float* data, uint32_t frames) {
            std::lock_guard<std::mutex> lock(bufferMutex);
            
            for (uint32_t i = 0; i < frames; ++i) {
                audioBuffer[writePos] = data[i];
                writePos = (writePos + 1) % bufferSize;
            }
            
            framesCapture += frames;
        });
        
        // Playback callback: Read from buffer to speakers
        playback->setCallback([&](float* data, uint32_t frames) {
            std::lock_guard<std::mutex> lock(bufferMutex);
            
            for (uint32_t i = 0; i < frames; ++i) {
                float sample = audioBuffer[readPos];
                readPos = (readPos + 1) % bufferSize;
                
                // Duplicate mono to stereo
                data[i * 2 + 0] = sample;  // Left
                data[i * 2 + 1] = sample;  // Right
            }
            
            framesPlayback += frames;
        });
        
        // Start both devices
        std::cout << "Starting capture..." << std::endl;
        capture->start();
        
        std::cout << "Starting playback..." << std::endl;
        playback->start();
        
        std::cout << "\n🎤 ECHO TEST ACTIVE 🔊" << std::endl;
        std::cout << "Speak into your microphone and hear yourself!" << std::endl;
        std::cout << "\nNote: If you hear feedback/squealing:" << std::endl;
        std::cout << "  - Use headphones" << std::endl;
        std::cout << "  - Reduce speaker volume" << std::endl;
        std::cout << "  - Move microphone away from speakers" << std::endl;
        std::cout << "\nPress Enter to stop..." << std::endl;
        std::cin.get();
        
        // Stop devices
        capture->stop();
        playback->stop();
        
        // Show statistics
        std::cout << "\n==================================================" << std::endl;
        std::cout << "Echo Test Statistics:" << std::endl;
        std::cout << "==================================================" << std::endl;
        std::cout << "  Frames Captured: " << framesCapture << std::endl;
        std::cout << "  Frames Played: " << framesPlayback << std::endl;
        
        double durationCapture = static_cast<double>(framesCapture) / captureConfig.sampleRateHz;
        double durationPlayback = static_cast<double>(framesPlayback) / playbackConfig.sampleRateHz;
        
        std::cout << "  Duration (Capture): " << std::fixed << std::setprecision(2)
                  << durationCapture << " seconds" << std::endl;
        std::cout << "  Duration (Playback): " << std::fixed << std::setprecision(2)
                  << durationPlayback << " seconds" << std::endl;
        
        size_t bufferFill = 0;
        if (writePos >= readPos) {
            bufferFill = writePos - readPos;
        } else {
            bufferFill = bufferSize - readPos + writePos;
        }
        
        std::cout << "  Buffer Fill: " << bufferFill << " samples ("
                  << std::fixed << std::setprecision(1)
                  << (bufferFill * 1000.0 / captureConfig.sampleRateHz) << " ms)" << std::endl;
        
        std::cout << "\n✅ Echo test completed successfully!" << std::endl;
        std::cout << "\nUse Cases for Full-Duplex:" << std::endl;
        std::cout << "  - VoIP/Video calling applications" << std::endl;
        std::cout << "  - Voice chat in games" << std::endl;
        std::cout << "  - Audio effects processing" << std::endl;
        std::cout << "  - Karaoke applications" << std::endl;
        std::cout << "  - Real-time audio analysis" << std::endl;
        
    } catch (const WasapiException& e) {
        std::cout << "\n❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

