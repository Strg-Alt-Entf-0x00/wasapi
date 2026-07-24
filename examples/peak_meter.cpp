/***************************************************************************
 * WASAPI Example: Real-Time Peak Meter
 *
 * Displays a live ASCII peak meter from the default playback device.
 * Uses IAudioMeterInformation for hardware-level peak detection.
 *
 * Usage: Play music while this runs to see the meter respond.
 ***************************************************************************/

#include <wasapi/device.h>
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <cmath>
#include <atomic>

static std::atomic<bool> g_running{true};

/// Render a horizontal ASCII peak meter bar.
static void printMeter(float peak) {
    constexpr int barWidth = 50;
    const int filled = static_cast<int>(peak * barWidth);

    // dB scale: 20 * log10(peak), clamped to -60 dB
    float dB = (peak > 0.0f) ? 20.0f * std::log10(peak) : -60.0f;
    if (dB < -60.0f) dB = -60.0f;

    std::cout << "\r  [";
    for (int i = 0; i < barWidth; ++i) {
        if (i < filled) {
            if (i >= barWidth - 5)      std::cout << '#';  // Red zone
            else if (i >= barWidth - 15) std::cout << '=';  // Yellow zone
            else                         std::cout << '-';  // Green zone
        } else {
            std::cout << ' ';
        }
    }
    std::cout << "] " << std::fixed << std::setprecision(1)
              << std::setw(6) << dB << " dB  " << std::flush;
}

int main() {
    std::cout << "WASAPI Peak Meter (Default Playback Device)\n";
    std::cout << "Play audio to see the meter. Press Enter to quit.\n\n";

    try {
        // Create a playback device (we only need it for peak metering)
        wasapi::DeviceConfig config;
        config.deviceType   = wasapi::DeviceType::Playback;
        config.sampleRateHz = 48000;
        config.channelCount = 2;

        auto device = wasapi::Device::create(config);

        // Set a no-op callback (required for start, but we only care about metering)
        device->setCallback([](float* buffer, uint32_t frameCount) {
            // Silence output - we are only observing the peak meter
            const size_t n = static_cast<size_t>(frameCount) * 2;
            std::fill_n(buffer, n, 0.0f);
        });

        device->start();

        // Spawn a reader thread for Enter key
        std::thread inputThread([]() {
            std::cin.get();
            g_running.store(false, std::memory_order_release);
        });

        // Poll peak meter at ~30 Hz
        while (g_running.load(std::memory_order_acquire)) {
            float peak = device->getPeakLevel();
            printMeter(peak);
            std::this_thread::sleep_for(std::chrono::milliseconds(33));
        }

        std::cout << "\n";
        device->stop();
        inputThread.join();

    } catch (const wasapi::WasapiException& e) {
        std::cerr << "WASAPI Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
