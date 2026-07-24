/***************************************************************************
 * WASAPI - Windows Audio Library
 *
 * Device Implementation v2.0
 *
 * Features:
 * - IAudioClient3 for low-latency shared mode
 * - Event-driven architecture (no polling, zero CPU idle)
 * - MMCSS "Pro Audio" thread priority
 * - RAII resource management
 * - Format negotiation with intelligent fallback
 * - Automatic format conversion (FormatConverter)
 * - AUTOCONVERTPCM for sample-rate-mismatched shared mode
 * - Per-session and per-channel volume control
 * - Peak level metering (IAudioMeterInformation)
 * - Session lifecycle events (IAudioSessionEvents)
 * - Stream position via IAudioClock
 * - Pre-allocated buffers (zero RT-thread allocation)
 * - Structured logging via WASAPI_LOG macros
 *
 * License: MIT
 * Author: Strg-Alt-Entf-0x00
 * Date: 2026-06-08
 * Updated: 2026-06-19 (v2.0.0 - full feature expansion)
 ***************************************************************************/

#include <wasapi/device.h>
#include "com_initializer.h"
#include "format_negotiator.h"
#include "format_converter.h"
#include "wasapi_log.h"

#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiopolicy.h>  // IAudioSessionEvents, IAudioSessionControl
#include <endpointvolume.h>  // IAudioMeterInformation
#include <avrt.h>
#include <functiondiscoverykeys_devpkey.h>
#include <wrl/client.h>
#include <wrl/implements.h>
#include <thread>
#include <atomic>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace wasapi {

// ===========================================================================
// SessionEventSink - COM class implementing IAudioSessionEvents
// ===========================================================================

/// Internal COM class: receives Windows audio session lifecycle notifications.
/// Invoked from a COM apartment thread - user callback must be thread-safe.
class SessionEventSink
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IAudioSessionEvents>
{
public:
    void setCallback(SessionEventCallback cb) noexcept { callback_ = std::move(cb); }

    // -- IAudioSessionEvents -------------------------------------------------

    HRESULT STDMETHODCALLTYPE OnDisplayNameChanged(
        LPCWSTR, LPCGUID) noexcept override { return S_OK; }

    HRESULT STDMETHODCALLTYPE OnIconPathChanged(
        LPCWSTR, LPCGUID) noexcept override { return S_OK; }

    HRESULT STDMETHODCALLTYPE OnSimpleVolumeChanged(
        float, BOOL, LPCGUID) noexcept override
    {
        dispatch(SessionEvent::VolumeChanged);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnChannelVolumeChanged(
        DWORD, float[], DWORD, LPCGUID) noexcept override
    {
        dispatch(SessionEvent::VolumeChanged);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnGroupingParamChanged(
        LPCGUID, LPCGUID) noexcept override { return S_OK; }

    HRESULT STDMETHODCALLTYPE OnStateChanged(
        AudioSessionState) noexcept override { return S_OK; }

    HRESULT STDMETHODCALLTYPE OnSessionDisconnected(
        AudioSessionDisconnectReason) noexcept override
    {
        dispatch(SessionEvent::Disconnected);
        return S_OK;
    }

private:
    void dispatch(SessionEvent event) noexcept {
        try {
            if (callback_) callback_(event);
        } catch (...) {}
    }

    SessionEventCallback callback_;
};

// ===========================================================================
// Device::Impl (PIMPL)
// ===========================================================================

struct Device::Impl {
    // -- Configuration -------------------------------------------------------
    DeviceConfig config_;
    DeviceInfo   deviceInfo_;

    // -- Format negotiation --------------------------------------------------
    internal::FormatNegotiator::NegotiationResult negotiatedFormat_{};

    // -- Core COM interfaces -------------------------------------------------
    internal::ComInitializer   comInit_;
    ComPtr<IMMDeviceEnumerator> deviceEnumerator_;
    ComPtr<IMMDevice>           device_;
    ComPtr<IAudioClient3>       audioClient_;
    ComPtr<IAudioRenderClient>  renderClient_;
    ComPtr<IAudioCaptureClient> captureClient_;

    // -- Optional COM interfaces (acquired best-effort) ----------------------
    ComPtr<ISimpleAudioVolume>     simpleVolume_;    // Per-session volume
    ComPtr<IAudioStreamVolume>     streamVolume_;    // Per-channel volume
    ComPtr<IAudioMeterInformation> meterInfo_;       // Peak metering
    ComPtr<IAudioClock>            audioClock_;       // Stream position
    ComPtr<IAudioSessionControl>   sessionControl_;  // Session events

    // -- Session events (COM callback) ----------------------------------------
    ComPtr<SessionEventSink> sessionSink_;
    SessionEventCallback     sessionCallback_;

    // -- Event-driven audio thread -------------------------------------------
    HANDLE              audioEvent_  = nullptr;
    std::thread         audioThread_;
    std::atomic<bool>   running_{false};
    std::atomic<bool>   shouldStop_{false};

    // -- MMCSS ---------------------------------------------------------------
    HANDLE mmcssHandle_ = nullptr;

    // -- Audio state ---------------------------------------------------------
    UINT32               bufferFrames_ = 0;
    std::atomic<uint32_t> underrunCount_{0};

    // -- Pre-allocated buffers (never allocate on RT thread) -----------------
    std::vector<float> conversionBuffer_;  // Used when format != Float32
    std::vector<float> silenceBuffer_;     // Used for silent capture packets

    // -- Callbacks -----------------------------------------------------------
    AudioCallback callback_;  // Set before start(), immutable during streaming

    // ========================================================================
    // Construction / Destruction
    // ========================================================================

    explicit Impl(const DeviceConfig& config) : config_(config) {
        if (!config_.isValid()) {
            throw WasapiException(ErrorCode::InvalidParameter,
                "Invalid device configuration");
        }
    }

    ~Impl() { cleanup(); }

    void cleanup() noexcept {
        // 1. Stop the audio thread
        if (running_.load(std::memory_order_acquire)) {
            shouldStop_.store(true, std::memory_order_release);
            if (audioEvent_) SetEvent(audioEvent_);
            if (audioThread_.joinable()) audioThread_.join();
            running_.store(false, std::memory_order_release);
        }

        // 2. Stop the audio client
        if (audioClient_) audioClient_->Stop();

        // 3. Revert MMCSS priority
        if (mmcssHandle_) {
            AvRevertMmThreadCharacteristics(mmcssHandle_);
            mmcssHandle_ = nullptr;
        }

        // 4. Unregister session event sink
        if (sessionControl_ && sessionSink_) {
            sessionControl_->UnregisterAudioSessionNotification(sessionSink_.Get());
        }

        // 5. Release COM interfaces (reverse acquisition order)
        sessionSink_.Reset();
        sessionControl_.Reset();
        audioClock_.Reset();
        meterInfo_.Reset();
        streamVolume_.Reset();
        simpleVolume_.Reset();
        captureClient_.Reset();
        renderClient_.Reset();
        audioClient_.Reset();
        device_.Reset();
        deviceEnumerator_.Reset();

        // 6. Close event handle
        if (audioEvent_) {
            CloseHandle(audioEvent_);
            audioEvent_ = nullptr;
        }

        // 7. Release pre-allocated buffers
        conversionBuffer_.clear();
        conversionBuffer_.shrink_to_fit();
        silenceBuffer_.clear();
        silenceBuffer_.shrink_to_fit();
    }

    // ========================================================================
    // Initialization: Phase 1 - Device acquisition
    // ========================================================================

    void initializeDevice() {
        HRESULT hr = CoCreateInstance(
            __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
            IID_PPV_ARGS(&deviceEnumerator_)
        );
        WASAPI_CHECK_HR(hr, "Failed to create device enumerator");

        // Map DeviceType to Windows data flow
        EDataFlow dataFlow;
        switch (config_.deviceType) {
            case DeviceType::Capture:  dataFlow = eCapture; break;
            case DeviceType::Loopback: dataFlow = eRender;  break; // Loopback = render endpoint
            default:                   dataFlow = eRender;  break;
        }

        // Acquire the IMMDevice
        if (config_.deviceId == L"default") {
            hr = deviceEnumerator_->GetDefaultAudioEndpoint(
                dataFlow, eConsole, &device_);
        } else {
            hr = deviceEnumerator_->GetDevice(config_.deviceId.c_str(), &device_);
        }
        WASAPI_CHECK_HR(hr, "Failed to get audio device");

        // Read device friendly name
        ComPtr<IPropertyStore> props;
        hr = device_->OpenPropertyStore(STGM_READ, &props);
        if (SUCCEEDED(hr)) {
            PROPVARIANT varName;
            PropVariantInit(&varName);
            if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &varName))) {
                deviceInfo_.name = varName.pwszVal ? varName.pwszVal : L"Unknown";
                PropVariantClear(&varName);
            }
        }
        deviceInfo_.id        = config_.deviceId;
        deviceInfo_.type      = config_.deviceType;
        deviceInfo_.isDefault = (config_.deviceId == L"default");

        WASAPI_LOG_INFO("Device acquired: %ls", deviceInfo_.name.c_str());
    }

    // ========================================================================
    // Initialization: Phase 2 - IAudioClient3 + format + services
    // ========================================================================

    void initializeAudioClient() {
        // Activate IAudioClient3 (Windows 10 1607+)
        HRESULT hr = device_->Activate(
            __uuidof(IAudioClient3), CLSCTX_ALL, nullptr,
            reinterpret_cast<void**>(audioClient_.GetAddressOf())
        );
        WASAPI_CHECK_HR(hr, "Failed to activate IAudioClient3");

        // Negotiate audio format (tries exact -> Float32 -> Int16 -> mix format)
        AUDCLNT_SHAREMODE shareMode = (config_.shareMode == ShareMode::Exclusive)
            ? AUDCLNT_SHAREMODE_EXCLUSIVE
            : AUDCLNT_SHAREMODE_SHARED;

        negotiatedFormat_ = internal::FormatNegotiator::negotiate(
            audioClient_, config_, shareMode);

        WASAPI_LOG_INFO("Format negotiated: %u Hz, %u ch, %s (conversion=%s)",
            negotiatedFormat_.negotiatedSampleRate,
            negotiatedFormat_.negotiatedChannels,
            getFormatName(negotiatedFormat_.negotiatedFormat),
            negotiatedFormat_.needsConversion ? "yes" : "no");

        // Buffer duration in 100-nanosecond units
        REFERENCE_TIME bufferDuration =
            static_cast<REFERENCE_TIME>(config_.bufferSizeMs) * 10000;

        // Stream flags
        DWORD streamFlags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;

        if (config_.deviceType == DeviceType::Loopback) {
            streamFlags |= AUDCLNT_STREAMFLAGS_LOOPBACK;
        }

        // Shared mode: enable automatic sample-rate/channel conversion
        // This lets the user request e.g. 16kHz mono for ASR even when the
        // system mix format is 48kHz stereo - Windows handles the SRC.
        if (shareMode == AUDCLNT_SHAREMODE_SHARED) {
            streamFlags |= AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM
                        |  AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
        }

        // Initialize the audio client
        hr = audioClient_->Initialize(
            shareMode,
            streamFlags,
            bufferDuration,
            (shareMode == AUDCLNT_SHAREMODE_EXCLUSIVE) ? bufferDuration : 0,
            reinterpret_cast<WAVEFORMATEX*>(&negotiatedFormat_.format),
            nullptr
        );
        WASAPI_CHECK_HR(hr, "Failed to initialize audio client");

        // Get actual buffer size (may differ from requested)
        hr = audioClient_->GetBufferSize(&bufferFrames_);
        WASAPI_CHECK_HR(hr, "Failed to get buffer size");

        WASAPI_LOG_INFO("Audio client initialized: %u frames buffer (%u ms requested)",
            bufferFrames_, config_.bufferSizeMs);

        // Pre-allocate conversion buffer (format conversion path)
        if (negotiatedFormat_.needsConversion) {
            const size_t maxSamples =
                static_cast<size_t>(bufferFrames_) * config_.channelCount;
            conversionBuffer_.resize(maxSamples, 0.0f);
        }

        // Pre-allocate silence buffer (capture path, zero RT allocation)
        silenceBuffer_.assign(
            static_cast<size_t>(bufferFrames_) * config_.channelCount, 0.0f);

        // -- Acquire sub-services from IAudioClient3 -------------------------

        if (config_.deviceType == DeviceType::Playback) {
            hr = audioClient_->GetService(
                __uuidof(IAudioRenderClient),
                reinterpret_cast<void**>(renderClient_.GetAddressOf()));
            WASAPI_CHECK_HR(hr, "Failed to get render client");
        } else {
            hr = audioClient_->GetService(
                __uuidof(IAudioCaptureClient),
                reinterpret_cast<void**>(captureClient_.GetAddressOf()));
            WASAPI_CHECK_HR(hr, "Failed to get capture client");
        }

        // Per-session volume (playback only, non-critical)
        if (config_.deviceType == DeviceType::Playback) {
            audioClient_->GetService(
                __uuidof(ISimpleAudioVolume),
                reinterpret_cast<void**>(simpleVolume_.GetAddressOf()));
        }

        // Per-channel volume (playback only, non-critical)
        if (config_.deviceType == DeviceType::Playback) {
            audioClient_->GetService(
                __uuidof(IAudioStreamVolume),
                reinterpret_cast<void**>(streamVolume_.GetAddressOf()));
        }

        // Peak metering (all device types, non-critical)
        // NOTE: IAudioMeterInformation is obtained from the IMMDevice, NOT
        // from IAudioClient. This is a common mistake.
        device_->Activate(
            __uuidof(IAudioMeterInformation), CLSCTX_ALL, nullptr,
            reinterpret_cast<void**>(meterInfo_.GetAddressOf()));

        // Audio clock (stream position, non-critical)
        audioClient_->GetService(
            __uuidof(IAudioClock),
            reinterpret_cast<void**>(audioClock_.GetAddressOf()));

        // Session control (for event registration, non-critical)
        audioClient_->GetService(
            __uuidof(IAudioSessionControl),
            reinterpret_cast<void**>(sessionControl_.GetAddressOf()));
    }

    // ========================================================================
    // Initialization: Phase 3 - Event handle + session events
    // ========================================================================

    void initializeEventDriven() {
        audioEvent_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!audioEvent_) {
            throw WasapiException(ErrorCode::InitializationFailed,
                "Failed to create audio event");
        }

        HRESULT hr = audioClient_->SetEventHandle(audioEvent_);
        WASAPI_CHECK_HR(hr, "Failed to set event handle");
    }

    void registerSessionEvents() {
        if (!sessionControl_ || !sessionCallback_) return;

        sessionSink_ = Microsoft::WRL::Make<SessionEventSink>();
        if (!sessionSink_) return;

        sessionSink_->setCallback(sessionCallback_);

        HRESULT hr = sessionControl_->RegisterAudioSessionNotification(
            sessionSink_.Get());
        if (SUCCEEDED(hr)) {
            WASAPI_LOG_DEBUG("Session event listener registered");
        }
    }

    // ========================================================================
    // Buffer priming (playback only)
    // ========================================================================

    void primeBuffer() {
        if (config_.deviceType != DeviceType::Playback || !renderClient_) return;

        BYTE* buffer = nullptr;
        HRESULT hr = renderClient_->GetBuffer(bufferFrames_, &buffer);
        if (SUCCEEDED(hr)) {
            renderClient_->ReleaseBuffer(bufferFrames_, AUDCLNT_BUFFERFLAGS_SILENT);
        }
    }

    // ========================================================================
    // Real-time audio thread
    // ========================================================================

    void audioThreadFunction() {
        // Elevate thread to "Pro Audio" MMCSS priority
        DWORD taskIndex = 0;
        mmcssHandle_ = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);
        if (!mmcssHandle_) {
            mmcssHandle_ = AvSetMmThreadCharacteristicsW(L"Audio", &taskIndex);
        }

        if (config_.deviceType == DeviceType::Playback) {
            playbackLoop();
        } else {
            captureLoop();
        }

        // Revert MMCSS on thread exit
        if (mmcssHandle_) {
            AvRevertMmThreadCharacteristics(mmcssHandle_);
            mmcssHandle_ = nullptr;
        }
    }

    // -- Playback loop -------------------------------------------------------

    void playbackLoop() {
        while (!shouldStop_.load(std::memory_order_acquire)) {
            DWORD result = WaitForSingleObject(audioEvent_, 100);

            if (result == WAIT_TIMEOUT) {
                underrunCount_.fetch_add(1, std::memory_order_relaxed);
                continue;
            }
            if (result != WAIT_OBJECT_0 || shouldStop_.load(std::memory_order_acquire)) {
                break;
            }

            UINT32 padding = 0;
            HRESULT hr = audioClient_->GetCurrentPadding(&padding);
            if (FAILED(hr)) continue;

            UINT32 availableFrames = bufferFrames_ - padding;
            if (availableFrames == 0) continue;

            BYTE* buffer = nullptr;
            hr = renderClient_->GetBuffer(availableFrames, &buffer);
            if (FAILED(hr)) continue;

            if (callback_) {
                callback_(reinterpret_cast<float*>(buffer), availableFrames);
            }

            renderClient_->ReleaseBuffer(availableFrames, 0);
        }
    }

    // -- Capture loop --------------------------------------------------------

    void captureLoop() {
        while (!shouldStop_.load(std::memory_order_acquire)) {
            DWORD result = WaitForSingleObject(audioEvent_, 100);

            if (result == WAIT_TIMEOUT) {
                underrunCount_.fetch_add(1, std::memory_order_relaxed);
                continue;
            }
            if (result != WAIT_OBJECT_0 || shouldStop_.load(std::memory_order_acquire)) {
                break;
            }

            UINT32 packetSize = 0;
            HRESULT hr = captureClient_->GetNextPacketSize(&packetSize);
            if (FAILED(hr)) continue;

            while (packetSize > 0) {
                BYTE*  buffer    = nullptr;
                UINT32 numFrames = 0;
                DWORD  flags     = 0;

                hr = captureClient_->GetBuffer(
                    &buffer, &numFrames, &flags, nullptr, nullptr);
                if (FAILED(hr)) break;

                const bool isSilent =
                    (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;

                // Dispatch to user callback (NO allocation on RT thread)
                if (callback_ && !isSilent) {
                    callback_(reinterpret_cast<float*>(buffer), numFrames);
                } else if (callback_ && isSilent) {
                    const size_t n =
                        static_cast<size_t>(numFrames) * config_.channelCount;
                    std::fill_n(silenceBuffer_.data(), n, 0.0f);
                    callback_(silenceBuffer_.data(), numFrames);
                }

                captureClient_->ReleaseBuffer(numFrames);

                hr = captureClient_->GetNextPacketSize(&packetSize);
                if (FAILED(hr)) break;
            }
        }
    }
};

// ===========================================================================
// Device public API
// ===========================================================================

// -- Factory -----------------------------------------------------------------

std::unique_ptr<Device> Device::create(const DeviceConfig& config) {
    auto device = std::unique_ptr<Device>(new Device(config));

    device->pimpl_->initializeDevice();
    device->pimpl_->initializeAudioClient();
    device->pimpl_->initializeEventDriven();

    WASAPI_LOG_INFO("Device created successfully");
    return device;
}

Device::Device(const DeviceConfig& config)
    : pimpl_(std::make_unique<Impl>(config)) {}

Device::~Device() = default;

Device::Device(Device&&) noexcept = default;
Device& Device::operator=(Device&&) noexcept = default;

// -- Core streaming ----------------------------------------------------------

bool Device::start() {
    if (pimpl_->running_.load(std::memory_order_acquire)) {
        return true;
    }
    if (!pimpl_->callback_) {
        throw WasapiException(ErrorCode::NotInitialized, "Callback not set");
    }

    // Register session events (if callback was set before start)
    pimpl_->registerSessionEvents();

    pimpl_->primeBuffer();

    HRESULT hr = pimpl_->audioClient_->Start();
    WASAPI_CHECK_HR(hr, "Failed to start audio client");

    pimpl_->shouldStop_.store(false, std::memory_order_release);
    pimpl_->running_.store(true, std::memory_order_release);

    pimpl_->audioThread_ = std::thread([this]() {
        pimpl_->audioThreadFunction();
    });

    WASAPI_LOG_INFO("Audio streaming started");
    return true;
}

void Device::stop() {
    if (!pimpl_->running_.load(std::memory_order_acquire)) return;

    pimpl_->shouldStop_.store(true, std::memory_order_release);

    if (pimpl_->audioEvent_) SetEvent(pimpl_->audioEvent_);

    if (pimpl_->audioThread_.joinable()) {
        pimpl_->audioThread_.join();
    }

    if (pimpl_->audioClient_) {
        pimpl_->audioClient_->Stop();
    }

    pimpl_->running_.store(false, std::memory_order_release);
    WASAPI_LOG_INFO("Audio streaming stopped");
}

void Device::reset() {
    const bool wasRunning = pimpl_->running_.load(std::memory_order_acquire);

    // Must stop the audio thread first for a safe reset
    if (wasRunning) {
        stop();
    }

    HRESULT hr = pimpl_->audioClient_->Reset();
    WASAPI_CHECK_HR(hr, "Failed to reset audio client");

    pimpl_->primeBuffer();
    pimpl_->underrunCount_.store(0, std::memory_order_relaxed);

    if (wasRunning) {
        start();
    }

    WASAPI_LOG_INFO("Audio client reset");
}

// -- Callbacks ---------------------------------------------------------------

void Device::setCallback(AudioCallback callback) {
    if (pimpl_->running_.load(std::memory_order_acquire)) {
        throw WasapiException(ErrorCode::AlreadyInitialized,
            "Cannot change callback while device is running. Call stop() first.");
    }
    pimpl_->callback_ = std::move(callback);
}

void Device::setSessionEventCallback(SessionEventCallback cb) {
    if (pimpl_->running_.load(std::memory_order_acquire)) {
        throw WasapiException(ErrorCode::AlreadyInitialized,
            "Cannot change session callback while device is running.");
    }
    pimpl_->sessionCallback_ = std::move(cb);
}

// -- Status ------------------------------------------------------------------

bool Device::isRunning() const noexcept {
    return pimpl_->running_.load(std::memory_order_acquire);
}

uint32_t Device::latencyMs() const noexcept {
    if (!pimpl_->audioClient_) return 0;

    UINT32 padding = 0;
    HRESULT hr = pimpl_->audioClient_->GetCurrentPadding(&padding);
    if (FAILED(hr)) return 0;

    return (padding * 1000) / pimpl_->config_.sampleRateHz;
}

DeviceInfo Device::deviceInfo() const {
    return pimpl_->deviceInfo_;
}

const DeviceConfig& Device::config() const noexcept {
    return pimpl_->config_;
}

uint32_t Device::underrunCount() const noexcept {
    return pimpl_->underrunCount_.load(std::memory_order_relaxed);
}

// -- Session volume ----------------------------------------------------------

void Device::setVolume(float level) {
    if (!pimpl_->simpleVolume_) {
        throw WasapiException(ErrorCode::NotInitialized,
            "Volume control not available");
    }
    HRESULT hr = pimpl_->simpleVolume_->SetMasterVolume(level, nullptr);
    WASAPI_CHECK_HR(hr, "Failed to set volume");
}

float Device::getVolume() const {
    if (!pimpl_->simpleVolume_) {
        throw WasapiException(ErrorCode::NotInitialized,
            "Volume control not available");
    }
    float level = 0.0f;
    HRESULT hr = pimpl_->simpleVolume_->GetMasterVolume(&level);
    WASAPI_CHECK_HR(hr, "Failed to get volume");
    return level;
}

void Device::setMuted(bool muted) {
    if (!pimpl_->simpleVolume_) {
        throw WasapiException(ErrorCode::NotInitialized,
            "Volume control not available");
    }
    HRESULT hr = pimpl_->simpleVolume_->SetMute(muted ? TRUE : FALSE, nullptr);
    WASAPI_CHECK_HR(hr, "Failed to set mute");
}

bool Device::isMuted() const {
    if (!pimpl_->simpleVolume_) {
        throw WasapiException(ErrorCode::NotInitialized,
            "Volume control not available");
    }
    BOOL muted = FALSE;
    HRESULT hr = pimpl_->simpleVolume_->GetMute(&muted);
    WASAPI_CHECK_HR(hr, "Failed to get mute state");
    return muted != FALSE;
}

// -- Per-channel volume ------------------------------------------------------

void Device::setChannelVolume(uint16_t channel, float level) {
    if (!pimpl_->streamVolume_) {
        throw WasapiException(ErrorCode::NotInitialized,
            "Per-channel volume control not available");
    }

    UINT32 channelCount = 0;
    HRESULT hr = pimpl_->streamVolume_->GetChannelCount(&channelCount);
    WASAPI_CHECK_HR(hr, "Failed to get channel count");

    if (channel >= channelCount) {
        throw WasapiException(ErrorCode::InvalidParameter,
            "Channel index out of range");
    }

    hr = pimpl_->streamVolume_->SetChannelVolume(channel, level);
    WASAPI_CHECK_HR(hr, "Failed to set channel volume");
}

float Device::getChannelVolume(uint16_t channel) const {
    if (!pimpl_->streamVolume_) {
        throw WasapiException(ErrorCode::NotInitialized,
            "Per-channel volume control not available");
    }

    UINT32 channelCount = 0;
    HRESULT hr = pimpl_->streamVolume_->GetChannelCount(&channelCount);
    WASAPI_CHECK_HR(hr, "Failed to get channel count");

    if (channel >= channelCount) {
        throw WasapiException(ErrorCode::InvalidParameter,
            "Channel index out of range");
    }

    float level = 0.0f;
    hr = pimpl_->streamVolume_->GetChannelVolume(channel, &level);
    WASAPI_CHECK_HR(hr, "Failed to get channel volume");
    return level;
}

uint16_t Device::getChannelCount() const noexcept {
    return pimpl_->config_.channelCount;
}

// -- Peak metering -----------------------------------------------------------

float Device::getPeakLevel() const noexcept {
    if (!pimpl_->meterInfo_) return 0.0f;

    float peak = 0.0f;
    HRESULT hr = pimpl_->meterInfo_->GetPeakValue(&peak);
    if (FAILED(hr)) return 0.0f;
    return peak;
}

// -- Stream position ---------------------------------------------------------

AudioPosition Device::getPosition() const noexcept {
    AudioPosition pos{0, 0, 0.0};
    if (!pimpl_->audioClock_) return pos;

    UINT64 frequency = 0;
    HRESULT hr = pimpl_->audioClock_->GetFrequency(&frequency);
    if (FAILED(hr) || frequency == 0) return pos;

    UINT64 position = 0;
    UINT64 qpc      = 0;
    hr = pimpl_->audioClock_->GetPosition(&position, &qpc);
    if (FAILED(hr)) return pos;

    // Convert from clock ticks to frames at the configured sample rate
    // GetFrequency() returns ticks/sec, GetPosition() returns ticks
    pos.frames      = (position * pimpl_->config_.sampleRateHz) / frequency;
    pos.qpcPosition = qpc;
    pos.seconds     = static_cast<double>(position) / static_cast<double>(frequency);
    return pos;
}

} // namespace wasapi
