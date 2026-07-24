/***************************************************************************
 * WASAPI Example - Device Enumeration
 * 
 * Lists all available audio devices
 * 
 * License: MIT
 ***************************************************************************/

#include <wasapi/device_enumerator.h>
#include <iostream>
#include <iomanip>

int main() {
    try {
        using namespace wasapi;
        
        std::wcout << L"=== WASAPI Device Enumeration Example ===\n\n";
        
        DeviceEnumerator enumerator;
        
        // Enumerate playback devices
        std::wcout << L"PLAYBACK DEVICES:\n";
        std::wcout << L"─────────────────────────────────────────\n";
        
        auto playbackDevices = enumerator.enumerateDevices(DeviceType::Playback);
        
        if (playbackDevices.empty()) {
            std::wcout << L"  No playback devices found.\n";
        } else {
            for (size_t i = 0; i < playbackDevices.size(); ++i) {
                const auto& device = playbackDevices[i];
                std::wcout << L"  [" << i << L"] " << device.name;
                if (device.isDefault) {
                    std::wcout << L" (DEFAULT)";
                }
                std::wcout << L"\n";
                std::wcout << L"      ID: " << device.id << L"\n";
            }
        }
        
        std::wcout << L"\n";
        
        // Enumerate capture devices
        std::wcout << L"CAPTURE DEVICES (Microphones):\n";
        std::wcout << L"─────────────────────────────────────────\n";
        
        auto captureDevices = enumerator.enumerateDevices(DeviceType::Capture);
        
        if (captureDevices.empty()) {
            std::wcout << L"  No capture devices found.\n";
        } else {
            for (size_t i = 0; i < captureDevices.size(); ++i) {
                const auto& device = captureDevices[i];
                std::wcout << L"  [" << i << L"] " << device.name;
                if (device.isDefault) {
                    std::wcout << L" (DEFAULT)";
                }
                std::wcout << L"\n";
                std::wcout << L"      ID: " << device.id << L"\n";
            }
        }
        
        std::wcout << L"\n";
        
        // Get default devices
        std::wcout << L"DEFAULT DEVICES:\n";
        std::wcout << L"─────────────────────────────────────────\n";
        
        try {
            auto defaultPlayback = enumerator.getDefaultDevice(DeviceType::Playback);
            std::wcout << L"  Playback: " << defaultPlayback.name << L"\n";
        } catch (...) {
            std::wcout << L"  Playback: Not available\n";
        }
        
        try {
            auto defaultCapture = enumerator.getDefaultDevice(DeviceType::Capture);
            std::wcout << L"  Capture:  " << defaultCapture.name << L"\n";
        } catch (...) {
            std::wcout << L"  Capture:  Not available\n";
        }
        
        std::wcout << L"\nDone!\n";
        
        return 0;
    }
    catch (const wasapi::WasapiException& e) {
        std::cerr << "WASAPI Error: " << e.what() << "\n";
        return 1;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
