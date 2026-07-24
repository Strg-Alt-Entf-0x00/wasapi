/***************************************************************************
 * WASAPI - Windows Audio Library
 *
 * Device Hot-Plug Watcher Implementation
 *
 * Wraps IMMNotificationClient to deliver device change events.
 * COM callbacks are forwarded directly to the user callback.
 * Thread-Safety: User callback invoked from COM notification thread.
 *
 * License: MIT
 * Author: AI-Projects Team
 * Date: 2026-06-19
 ***************************************************************************/

#include <wasapi/device_watcher.h>
#include "com_initializer.h"
#include "wasapi_log.h"

#include <mmdeviceapi.h>
#include <wrl/client.h>
#include <wrl/implements.h>
#include <atomic>

using Microsoft::WRL::ComPtr;

namespace wasapi {

// ---------------------------------------------------------------------------
// COM implementation of IMMNotificationClient
// ---------------------------------------------------------------------------

/// Internal COM class - hidden from public API via PIMPL.
class DeviceNotificationClient
    : public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMMNotificationClient>
{
public:
    void setCallback(DeviceChangeCallback cb) noexcept {
        callback_ = std::move(cb);
    }

    // IMMNotificationClient -----------------------------------------------

    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(
        LPCWSTR deviceId, DWORD /*newState*/) noexcept override
    {
        dispatch({
            deviceId ? deviceId : L"",
            DeviceEvent::StateChanged,
            DeviceType::Playback,  // State unknown without extra query
            false
        });
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR deviceId) noexcept override {
        dispatch({
            deviceId ? deviceId : L"",
            DeviceEvent::Added,
            DeviceType::Playback,
            false
        });
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR deviceId) noexcept override {
        dispatch({
            deviceId ? deviceId : L"",
            DeviceEvent::Removed,
            DeviceType::Playback,
            false
        });
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(
        EDataFlow flow, ERole /*role*/, LPCWSTR deviceId) noexcept override
    {
        // Map EDataFlow to DeviceType (ignore Loopback - same endpoint as Playback)
        const DeviceType type = (flow == eCapture)
            ? DeviceType::Capture
            : DeviceType::Playback;

        dispatch({
            deviceId ? deviceId : L"",
            DeviceEvent::DefaultChanged,
            type,
            true
        });
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(
        LPCWSTR /*deviceId*/, const PROPERTYKEY /*key*/) noexcept override
    {
        // Not forwarded - property changes are too noisy for most use cases.
        return S_OK;
    }

private:
    void dispatch(const DeviceChangeInfo& info) noexcept {
        try {
            if (callback_) {
                callback_(info);
            }
        } catch (...) {
            // Swallow - must not propagate from COM callback
        }
    }

    DeviceChangeCallback callback_;
};

// ---------------------------------------------------------------------------
// DeviceWatcher PIMPL
// ---------------------------------------------------------------------------

struct DeviceWatcher::Impl {
    internal::ComInitializer        comInit_;
    ComPtr<IMMDeviceEnumerator>     enumerator_;
    ComPtr<DeviceNotificationClient> notifier_;
    DeviceChangeCallback            callback_;
    std::atomic<bool>               watching_{false};

    explicit Impl(DeviceChangeCallback callback)
        : callback_(std::move(callback))
    {
        // Create IMMDeviceEnumerator (needed for registration)
        HRESULT hr = CoCreateInstance(
            __uuidof(MMDeviceEnumerator),
            nullptr,
            CLSCTX_ALL,
            IID_PPV_ARGS(&enumerator_)
        );
        WASAPI_CHECK_HR(hr, "DeviceWatcher: Failed to create IMMDeviceEnumerator");
    }

    ~Impl() {
        stopInternal();
    }

    void startInternal() {
        if (watching_.load(std::memory_order_acquire)) return;

        notifier_ = Microsoft::WRL::Make<DeviceNotificationClient>();
        if (!notifier_) {
            throw WasapiException(ErrorCode::AllocationFailed,
                "DeviceWatcher: Failed to allocate DeviceNotificationClient");
        }
        notifier_->setCallback(callback_);

        HRESULT hr = enumerator_->RegisterEndpointNotificationCallback(notifier_.Get());
        WASAPI_CHECK_HR(hr, "DeviceWatcher: Failed to register endpoint notification");

        watching_.store(true, std::memory_order_release);
        WASAPI_LOG_INFO("DeviceWatcher: started - monitoring all audio endpoints");
    }

    void stopInternal() noexcept {
        if (!watching_.exchange(false, std::memory_order_acq_rel)) return;

        if (enumerator_ && notifier_) {
            enumerator_->UnregisterEndpointNotificationCallback(notifier_.Get());
        }
        notifier_.Reset();
        WASAPI_LOG_INFO("DeviceWatcher: stopped");
    }
};

// ---------------------------------------------------------------------------
// DeviceWatcher public API
// ---------------------------------------------------------------------------

DeviceWatcher::DeviceWatcher(DeviceChangeCallback callback)
    : pimpl_(std::make_unique<Impl>(std::move(callback)))
{
    pimpl_->startInternal();
}

DeviceWatcher::~DeviceWatcher() = default;

DeviceWatcher::DeviceWatcher(DeviceWatcher&&) noexcept = default;
DeviceWatcher& DeviceWatcher::operator=(DeviceWatcher&&) noexcept = default;

void DeviceWatcher::stop() noexcept {
    if (pimpl_) pimpl_->stopInternal();
}

void DeviceWatcher::start() {
    if (pimpl_) pimpl_->startInternal();
}

bool DeviceWatcher::isWatching() const noexcept {
    return pimpl_ && pimpl_->watching_.load(std::memory_order_acquire);
}

} // namespace wasapi
