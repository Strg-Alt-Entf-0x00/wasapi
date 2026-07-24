/***************************************************************************
 * WASAPI Example: Hot-Plug Device Monitoring
 *
 * Demonstrates DeviceWatcher for real-time audio endpoint change detection.
 * Run this, then plug/unplug headphones or change the Windows default device.
 ***************************************************************************/

#include <wasapi/device_watcher.h>
#include <iostream>
#include <string>

static const char* eventName(wasapi::DeviceEvent event) {
    switch (event) {
        case wasapi::DeviceEvent::Added:          return "ADDED";
        case wasapi::DeviceEvent::Removed:        return "REMOVED";
        case wasapi::DeviceEvent::DefaultChanged: return "DEFAULT_CHANGED";
        case wasapi::DeviceEvent::StateChanged:   return "STATE_CHANGED";
        default:                                  return "UNKNOWN";
    }
}

int main() {
    std::cout << "WASAPI Hot-Plug Monitor\n";
    std::cout << "Watching for audio device changes...\n";
    std::cout << "Plug/unplug devices or change Windows default. Press Enter to quit.\n\n";

    try {
        wasapi::DeviceWatcher watcher([](const wasapi::DeviceChangeInfo& info) {
            std::wcout << L"[" << eventName(info.event) << L"] "
                       << L"Device: " << info.deviceId << L"\n";
        });

        std::cin.get();
        watcher.stop();
    } catch (const wasapi::WasapiException& e) {
        std::cerr << "WASAPI Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
