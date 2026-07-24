#pragma once
/***************************************************************************
 * WASAPI - Windows Audio Library
 * 
 * Device Enumeration
 * 
 * License: MIT
 * Author: AI-Projects Team
 * Date: 2026-06-08
 ***************************************************************************/

#include "types.h"
#include "error.h"
#include <memory>
#include <vector>

namespace wasapi {

/**
 * @brief Audio device enumerator
 * 
 * Discovers available audio devices on the system.
 * Uses IMMDeviceEnumerator for device discovery.
 * 
 * Thread-Safety: All methods are thread-safe
 * 
 * Example Usage:
 * @code
 * DeviceEnumerator enumerator;
 * 
 * // List all playback devices
 * auto playbackDevices = enumerator.enumerateDevices(DeviceType::Playback);
 * for (const auto& device : playbackDevices) {
 *     std::wcout << device.name << L" (Default: " << device.isDefault << L")\n";
 * }
 * 
 * // Get default microphone
 * auto defaultMic = enumerator.getDefaultDevice(DeviceType::Capture);
 * @endcode
 */
class DeviceEnumerator {
public:
    /**
     * @brief Constructor
     * 
     * Initializes COM and creates device enumerator.
     * 
     * @throws WasapiException if COM initialization fails
     */
    DeviceEnumerator();
    
    /**
     * @brief Destructor
     * 
     * Releases COM resources
     */
    ~DeviceEnumerator();
    
    // Non-copyable, movable
    DeviceEnumerator(const DeviceEnumerator&) = delete;
    DeviceEnumerator& operator=(const DeviceEnumerator&) = delete;
    DeviceEnumerator(DeviceEnumerator&&) noexcept;
    DeviceEnumerator& operator=(DeviceEnumerator&&) noexcept;
    
    /**
     * @brief Enumerate all devices of specified type
     * 
     * @param type Device type (Playback, Capture, Loopback)
     * @return std::vector<DeviceInfo> List of available devices
     * @throws WasapiException if enumeration fails
     * 
     * Note: Loopback devices are the same as Playback devices but
     * configured for capturing system audio ("What You Hear")
     */
    [[nodiscard]] std::vector<DeviceInfo> enumerateDevices(DeviceType type);
    
    /**
     * @brief Get default device
     * 
     * @param type Device type (Playback, Capture, Loopback)
     * @return DeviceInfo Default device information
     * @throws WasapiException if no default device found
     * 
     * Returns the Windows default audio endpoint for the specified role.
     * This is the device the user has selected in Windows Sound settings.
     */
    [[nodiscard]] DeviceInfo getDefaultDevice(DeviceType type);
    
    /**
     * @brief Get device by ID
     * 
     * @param deviceId Device identifier (from DeviceInfo::id)
     * @return DeviceInfo Device information
     * @throws WasapiException if device not found
     * 
     * Useful for reopening a specific device that was previously enumerated.
     */
    [[nodiscard]] DeviceInfo getDeviceById(const std::wstring& deviceId);
    
    /**
     * @brief Check if device exists
     * 
     * @param deviceId Device identifier
     * @return true if device exists and is enabled
     * 
     * Useful for validating stored device IDs before attempting to open.
     */
    [[nodiscard]] bool deviceExists(const std::wstring& deviceId) noexcept;
    
    /**
     * @brief Refresh device list
     * 
     * Clears internal cache and forces re-enumeration.
     * Call this after device hot-plug events.
     */
    void refresh();

private:
    /**
     * @brief Implementation details (PIMPL idiom)
     */
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace wasapi
