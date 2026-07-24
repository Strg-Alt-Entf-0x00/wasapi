/***************************************************************************
 * WASAPI Example: Session Event Monitoring
 *
 * Demonstrates IAudioSessionEvents integration.
 * Detects when Windows disconnects the audio session (device unplug,
 * exclusive-mode takeover, service restart, etc.).
 *
 * Usage: Start this, then unplug your audio device or change the default.
 ***************************************************************************/

#include <wasapi/device.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>

static std::atomic<bool> g_running{true};
static std::atomic<bool> g_disconnected{false};

int main() {
    std::cout << "WASAPI Session Event Monitor\n";
    std::cout << "Streaming silence on default playback device.\n";
    std::cout << "Unplug the device or change defaults to trigger events.\n";
    std::cout << "Press Enter to quit.\n\n";

    try {
        wasapi::DeviceConfig config;
        config.deviceType   = wasapi::DeviceType::Playback;
        config.sampleRateHz = 48000;
        config.channelCount = 2;
        config.bufferSizeMs = wasapi::toMs(wasapi::LatencyMode::Normal);

        auto device = wasapi::Device::create(config);

        // Register session event callback BEFORE start()
        device->setSessionEventCallback([](wasapi::SessionEvent event) {
            switch (event) {
                case wasapi::SessionEvent::Disconnected:
                    std::cout << "\n  [EVENT] Session DISCONNECTED - device lost!\n";
                    g_disconnected.store(true, std::memory_order_release);
                    break;
                case wasapi::SessionEvent::FormatChanged:
                    std::cout << "\n  [EVENT] Format changed\n";
                    break;
                case wasapi::SessionEvent::VolumeChanged:
                    std::cout << "\n  [EVENT] Volume changed externally\n";
                    break;
            }
        });

        // Silence callback
        device->setCallback([](float* buffer, uint32_t frameCount) {
            std::fill_n(buffer, static_cast<size_t>(frameCount) * 2, 0.0f);
        });

        device->start();

        // Spawn Enter-key reader
        std::thread inputThread([]() {
            std::cin.get();
            g_running.store(false, std::memory_order_release);
        });

        // Main loop: print position + underrun status
        while (g_running.load(std::memory_order_acquire) &&
               !g_disconnected.load(std::memory_order_acquire))
        {
            auto pos = device->getPosition();
            std::cout << "\r  Position: " << pos.seconds << " s  |  "
                      << "Underruns: " << device->underrunCount()
                      << "  " << std::flush;
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        std::cout << "\n";
        device->stop();

        if (g_disconnected.load(std::memory_order_acquire)) {
            std::cout << "Session was disconnected. Exiting.\n";
        }

        g_running.store(false, std::memory_order_release);
        inputThread.join();

    } catch (const wasapi::WasapiException& e) {
        std::cerr << "WASAPI Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
