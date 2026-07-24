# WASAPI - Windows Audio Library

[![Version](https://img.shields.io/badge/version-0.9.2-blue.svg)](https://github.com/Strg-Alt-Entf-0x00/wasapi)
[![License](https://img.shields.io/badge/license-Public%20Domain-green.svg)](LICENSE)
[![C++](https://img.shields.io/badge/C++-20-orange.svg)](https://isocpp.org/)
[![Platform](https://img.shields.io/badge/platform-Windows-lightgrey.svg)](https://www.microsoft.com/windows)

WASAPI (Windows Audio Session API) implementation for audio applications. Built with C++20 for low-latency audio processing.

## Features

- 🎵 **IAudioClient3** - Low-latency shared and exclusive modes
- ⚡ **Event-Driven Architecture** - Zero CPU polling, pure event-driven callbacks
- 🎯 **MMCSS Integration** - "Pro Audio" thread priority for real-time performance
- 🔌 **Hot-Plug Support** - Automatic device monitoring with IMMNotificationClient
- 📊 **Audio Sessions** - Full session lifecycle events via IAudioSessionEvents
- 📈 **Peak Metering** - Per-channel volume monitoring
- 🔄 **Sample Rate Conversion** - Automatic format conversion (AUTOCONVERTPCM)
- 🏗️ **Modern C++20** - RAII, PIMPL pattern, concepts, and strong type safety

## Requirements

- **CMake**: 3.20 or higher
- **Compiler**: C++20 support required
  - MSVC 2019 16.11+ (Visual Studio 2019)
  - MSVC 2022 (recommended)
- **Platform**: Windows 10 1809+ or Windows 11
- **Windows SDK**: 10.0.17763.0 or higher

### Linked Libraries
- `Ole32.lib` - COM infrastructure
- `Avrt.lib` - Multimedia Class Scheduler Service (MMCSS)

## Quick Start

### Building

```bash
# Using the provided build script
build.bat

# Or manually
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
cmake --install . --prefix ../install
```

### Basic Usage - Device Enumeration

```cpp
#include <wasapi/device_enumerator.h>
#include <iostream>

int main() {
    wasapi::DeviceEnumerator enumerator;
    
    // List all audio devices
    auto devices = enumerator.enumerate_all();
    
    for (const auto& device : devices) {
        std::wcout << L"Device: " << device.name << L"\n";
        std::wcout << L"  ID: " << device.id << L"\n";
        std::wcout << L"  Type: " << (device.is_capture ? L"Input" : L"Output") << L"\n";
        std::wcout << L"  Sample Rate: " << device.sample_rate << L" Hz\n";
        std::wcout << L"  Channels: " << device.channels << L"\n\n";
    }
    
    return 0;
}
```

### Audio Playback

```cpp
#include <wasapi/device.h>

int main() {
    // Open default output device
    wasapi::Device device(wasapi::DeviceType::Render, true);
    
    // Set audio callback
    device.set_audio_callback([](float* buffer, size_t frames, size_t channels) {
        // Fill buffer with audio data
        for (size_t i = 0; i < frames * channels; ++i) {
            buffer[i] = generate_audio_sample();
        }
    });
    
    // Start playback
    device.start();
    
    // ... do work ...
    
    device.stop();
    return 0;
}
```

### Audio Recording

```cpp
#include <wasapi/device.h>

int main() {
    // Open default input device
    wasapi::Device device(wasapi::DeviceType::Capture, true);
    
    // Set audio callback
    device.set_audio_callback([](const float* buffer, size_t frames, size_t channels) {
        // Process recorded audio
        process_audio(buffer, frames, channels);
    });
    
    // Start recording
    device.start();
    
    // ... do work ...
    
    device.stop();
    return 0;
}
```

### Hot-Plug Device Monitoring

```cpp
#include <wasapi/device_watcher.h>

int main() {
    wasapi::DeviceWatcher watcher;
    
    // Register callbacks
    watcher.on_device_added([](const std::wstring& device_id) {
        std::wcout << L"Device added: " << device_id << L"\n";
    });
    
    watcher.on_device_removed([](const std::wstring& device_id) {
        std::wcout << L"Device removed: " << device_id << L"\n";
    });
    
    watcher.on_default_device_changed([](wasapi::DeviceType type) {
        std::wcout << L"Default device changed\n";
    });
    
    // ... application runs ...
    
    return 0;
}
```

## CMake Integration

```cmake
# In your CMakeLists.txt
add_subdirectory(third-party-cpp/wasapi-0.9.2)

add_executable(your_audio_app main.cpp)
target_link_libraries(your_audio_app PRIVATE wasapi)
```

## Configuration Options

| Option | Default | Description |
|--------|---------|-------------|
| `WASAPI_BUILD_EXAMPLES` | `OFF` | Build example programs |
| `WASAPI_BUILD_TESTS` | `OFF` | Build unit tests |

Build with examples:
```bash
cmake -DWASAPI_BUILD_EXAMPLES=ON ..
```

## Examples

The library includes several example programs:

- `enumerate_devices.cpp` - List all audio devices
- `playback_simple.cpp` - Basic audio playback
- `record_microphone.cpp` - Microphone recording
- `record_loopback.cpp` - System audio capture
- `full_duplex_echo.cpp` - Simultaneous I/O
- `peak_meter.cpp` - Real-time audio level monitoring
- `hot_plug_demo.cpp` - Device connection/disconnection handling
- `session_events.cpp` - Audio session lifecycle tracking

Build examples:
```bash
cd build
cmake -DWASAPI_BUILD_EXAMPLES=ON ..
cmake --build . --config Release
```

## Architecture

### Key Components

- **Device** - Core audio device abstraction with IAudioClient3
- **DeviceEnumerator** - Device discovery and querying
- **DeviceWatcher** - Hot-plug monitoring
- **FormatConverter** - Automatic sample format conversion
- **FormatNegotiator** - Format matching and negotiation

### Threading Model

- Main thread: Device control, enumeration
- Audio thread: Real-time audio callbacks with MMCSS "Pro Audio" priority
- Event thread: Hot-plug notifications and session events

## Performance Considerations

- Uses **IAudioClient3** for minimal latency (2-10ms possible)
- **MMCSS** ensures real-time thread scheduling
- Event-driven callbacks eliminate polling overhead
- Lock-free audio callbacks for maximum performance
- Automatic buffer management

## Project Structure

```
wasapi-0.9.2/
├── include/
│   └── wasapi/
│       ├── device.h
│       ├── device_enumerator.h
│       ├── device_watcher.h
│       └── types.h
├── src/
│   ├── device.cpp
│   ├── device_enumerator.cpp
│   ├── device_watcher.cpp
│   ├── format_converter.cpp
│   ├── format_negotiator.cpp
│   └── logger.cpp
├── examples/
├── build.bat
├── CMakeLists.txt
└── README.md
```

## Troubleshooting

### Audio Glitches
- Ensure your audio callback is lock-free
- Avoid memory allocations in the audio thread
- Check CPU usage and system load

### Device Not Found
- Verify device is enabled in Windows Sound settings
- Check device permissions
- Ensure Windows Audio service is running

## License

This is free and unencumbered software released into the **public domain**.

Anyone is free to copy, modify, publish, use, compile, sell, or distribute this software, either in source code form or as a compiled binary, for any purpose, commercial or non-commercial, and by any means.

See [LICENSE](LICENSE) or <http://unlicense.org/> for details.

