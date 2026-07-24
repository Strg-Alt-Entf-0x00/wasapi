/***************************************************************************
 * WASAPI - Windows Audio Library
 * 
 * Device Enumeration Implementation
 * 
 * License: MIT
 * Author: Strg-Alt-Entf-0x00
 * Date: 2026-06-08
 ***************************************************************************/

#include <wasapi/device_enumerator.h>
#include "com_initializer.h"
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <wrl/client.h>
#include <mutex>

using Microsoft::WRL::ComPtr;

namespace wasapi {

// PIMPL implementation
struct DeviceEnumerator::Impl {
    internal::ComInitializer comInit_;
    ComPtr<IMMDeviceEnumerator> enumerator_;
    std::mutex mutex_;
    
    Impl() {
        // Create device enumerator
        HRESULT hr = CoCreateInstance(
            __uuidof(MMDeviceEnumerator),
            nullptr,
            CLSCTX_ALL,
            IID_PPV_ARGS(&enumerator_)
        );
        
        WASAPI_CHECK_HR(hr, "Failed to create device enumerator");
    }
    
    [[nodiscard]] static std::wstring getDeviceName(IMMDevice* device) {
        ComPtr<IPropertyStore> props;
        HRESULT hr = device->OpenPropertyStore(STGM_READ, &props);
        if (FAILED(hr)) {
            return L"Unknown Device";
        }
        
        PROPVARIANT varName;
        PropVariantInit(&varName);
        hr = props->GetValue(PKEY_Device_FriendlyName, &varName);
        if (FAILED(hr)) {
            return L"Unknown Device";
        }
        
        std::wstring name = varName.pwszVal ? varName.pwszVal : L"Unknown Device";
        PropVariantClear(&varName);
        return name;
    }
    
    [[nodiscard]] static std::wstring getDeviceId(IMMDevice* device) {
        LPWSTR id = nullptr;
        HRESULT hr = device->GetId(&id);
        if (FAILED(hr) || !id) {
            return L"";
        }
        
        std::wstring deviceId = id;
        CoTaskMemFree(id);
        return deviceId;
    }
    
    [[nodiscard]] static EDataFlow getDataFlow(DeviceType type) noexcept {
        switch (type) {
            case DeviceType::Playback:
            case DeviceType::Loopback:
                return eRender;
            case DeviceType::Capture:
                return eCapture;
            default:
                return eRender;
        }
    }
    
    [[nodiscard]] static ERole getRole() noexcept {
        return eConsole;  // Default Windows audio endpoint
    }
};

DeviceEnumerator::DeviceEnumerator()
    : pimpl_(std::make_unique<Impl>())
{
}

DeviceEnumerator::~DeviceEnumerator() = default;

DeviceEnumerator::DeviceEnumerator(DeviceEnumerator&&) noexcept = default;
DeviceEnumerator& DeviceEnumerator::operator=(DeviceEnumerator&&) noexcept = default;

std::vector<DeviceInfo> DeviceEnumerator::enumerateDevices(DeviceType type) {
    std::lock_guard<std::mutex> lock(pimpl_->mutex_);
    
    std::vector<DeviceInfo> devices;
    
    // Get device collection
    ComPtr<IMMDeviceCollection> collection;
    HRESULT hr = pimpl_->enumerator_->EnumAudioEndpoints(
        Impl::getDataFlow(type),
        DEVICE_STATE_ACTIVE,
        &collection
    );
    
    WASAPI_CHECK_HR(hr, "Failed to enumerate devices");
    
    // Get device count
    UINT count = 0;
    hr = collection->GetCount(&count);
    WASAPI_CHECK_HR(hr, "Failed to get device count");
    
    // Get default device ID for comparison
    ComPtr<IMMDevice> defaultDevice;
    hr = pimpl_->enumerator_->GetDefaultAudioEndpoint(
        Impl::getDataFlow(type),
        Impl::getRole(),
        &defaultDevice
    );
    
    std::wstring defaultDeviceId;
    if (SUCCEEDED(hr)) {
        defaultDeviceId = Impl::getDeviceId(defaultDevice.Get());
    }
    
    // Enumerate devices
    devices.reserve(count);
    for (UINT i = 0; i < count; ++i) {
        ComPtr<IMMDevice> device;
        hr = collection->Item(i, &device);
        if (FAILED(hr)) continue;
        
        DeviceInfo info;
        info.id = Impl::getDeviceId(device.Get());
        info.name = Impl::getDeviceName(device.Get());
        info.type = type;
        info.isDefault = (info.id == defaultDeviceId);
        
        devices.push_back(std::move(info));
    }
    
    return devices;
}

DeviceInfo DeviceEnumerator::getDefaultDevice(DeviceType type) {
    std::lock_guard<std::mutex> lock(pimpl_->mutex_);
    
    ComPtr<IMMDevice> device;
    HRESULT hr = pimpl_->enumerator_->GetDefaultAudioEndpoint(
        Impl::getDataFlow(type),
        Impl::getRole(),
        &device
    );
    
    WASAPI_CHECK_HR(hr, "Failed to get default device");
    
    DeviceInfo info;
    info.id = Impl::getDeviceId(device.Get());
    info.name = Impl::getDeviceName(device.Get());
    info.type = type;
    info.isDefault = true;
    
    return info;
}

DeviceInfo DeviceEnumerator::getDeviceById(const std::wstring& deviceId) {
    std::lock_guard<std::mutex> lock(pimpl_->mutex_);
    
    ComPtr<IMMDevice> device;
    HRESULT hr = pimpl_->enumerator_->GetDevice(deviceId.c_str(), &device);
    
    WASAPI_CHECK_HR(hr, "Failed to get device by ID");
    
    // Check device state
    DWORD state = 0;
    hr = device->GetState(&state);
    if (FAILED(hr) || state != DEVICE_STATE_ACTIVE) {
        throw WasapiException(ErrorCode::DeviceNotFound, "Device is not active");
    }
    
    DeviceInfo info;
    info.id = deviceId;
    info.name = Impl::getDeviceName(device.Get());
    info.type = DeviceType::Playback;  // Type unknown from ID alone
    info.isDefault = false;
    
    return info;
}

bool DeviceEnumerator::deviceExists(const std::wstring& deviceId) noexcept {
    try {
        std::lock_guard<std::mutex> lock(pimpl_->mutex_);
        
        ComPtr<IMMDevice> device;
        HRESULT hr = pimpl_->enumerator_->GetDevice(deviceId.c_str(), &device);
        if (FAILED(hr)) {
            return false;
        }
        
        DWORD state = 0;
        hr = device->GetState(&state);
        return SUCCEEDED(hr) && state == DEVICE_STATE_ACTIVE;
    }
    catch (...) {
        return false;
    }
}

void DeviceEnumerator::refresh() {
    // No internal cache to clear, each call queries fresh
    // This is a no-op but provided for API consistency
}

} // namespace wasapi
