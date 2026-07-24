#pragma once
/***************************************************************************
 * WASAPI - Windows Audio Library
 *
 * Error Handling
 *
 * License: MIT
 * Author: Strg-Alt-Entf-0x00
 * Date: 2026-06-08
 * Updated: 2026-06-19 (v2.0.0 - DeviceDisconnected, SessionExpired)
 ***************************************************************************/

#include <stdexcept>
#include <string>
#include <system_error>
#include <windows.h>
#include <audioclient.h>  // For WASAPI error codes

namespace wasapi {

/**
 * @brief WASAPI error codes
 */
enum class ErrorCode {
    Success = 0,
    DeviceNotFound,
    DeviceInUse,
    DeviceDisconnected,    ///< NEW v2.0: Device was unplugged or session forcibly ended
    InvalidFormat,
    InvalidParameter,
    InitializationFailed,
    AllocationFailed,
    NotInitialized,
    AlreadyInitialized,
    Timeout,
    BufferUnderrun,
    BufferOverrun,
    SessionExpired,        ///< NEW v2.0: Windows audio session terminated (e.g. exclusive-mode stolen)
    ComError,
    UnknownError
};

/**
 * @brief WASAPI exception class
 *
 * Thrown when WASAPI operations fail.
 * Carries both a typed ErrorCode and (optionally) the raw HRESULT.
 */
class WasapiException : public std::runtime_error {
public:
    /// Construct from a high-level error code.
    explicit WasapiException(ErrorCode code, const std::string& message)
        : std::runtime_error(message)
        , errorCode_(code)
    {}

    /// Construct from a Windows HRESULT (error code is derived automatically).
    explicit WasapiException(HRESULT hr, const std::string& context)
        : std::runtime_error(formatHresultMessage(hr, context))
        , errorCode_(hresultToErrorCode(hr))
        , hresult_(hr)
    {}

    /// Returns the typed error code.
    [[nodiscard]] ErrorCode code()    const noexcept { return errorCode_; }

    /// Returns the raw HRESULT, or S_OK if not constructed from an HRESULT.
    [[nodiscard]] HRESULT   hresult() const noexcept { return hresult_; }

private:
    ErrorCode errorCode_;
    HRESULT   hresult_ = S_OK;

    [[nodiscard]] static ErrorCode hresultToErrorCode(HRESULT hr) noexcept {
        if (hr == AUDCLNT_E_DEVICE_IN_USE)          return ErrorCode::DeviceInUse;
        if (hr == AUDCLNT_E_UNSUPPORTED_FORMAT)      return ErrorCode::InvalidFormat;
        if (hr == AUDCLNT_E_NOT_INITIALIZED)         return ErrorCode::NotInitialized;
        if (hr == AUDCLNT_E_ALREADY_INITIALIZED)     return ErrorCode::AlreadyInitialized;
        if (hr == AUDCLNT_E_DEVICE_INVALIDATED)      return ErrorCode::DeviceDisconnected;
        if (hr == AUDCLNT_E_SERVICE_NOT_RUNNING)     return ErrorCode::DeviceDisconnected;
        if (hr == AUDCLNT_E_BUFFER_ERROR ||
            hr == AUDCLNT_E_BUFFER_SIZE_ERROR)       return ErrorCode::InvalidParameter;
        if (hr == AUDCLNT_E_OUT_OF_ORDER)            return ErrorCode::InitializationFailed;
        if (hr == E_OUTOFMEMORY)                     return ErrorCode::AllocationFailed;
        if (hr == E_INVALIDARG || hr == E_POINTER)   return ErrorCode::InvalidParameter;
        return ErrorCode::ComError;
    }

    [[nodiscard]] static std::string formatHresultMessage(HRESULT hr, const std::string& context) {
        char hexStr[16];
        snprintf(hexStr, sizeof(hexStr), "0x%08lX", static_cast<unsigned long>(hr));

        std::string message = context + " failed with HRESULT: " + hexStr;

        if (hr == AUDCLNT_E_DEVICE_IN_USE)       message += " (Device in use)";
        else if (hr == AUDCLNT_E_UNSUPPORTED_FORMAT)  message += " (Unsupported format)";
        else if (hr == AUDCLNT_E_NOT_INITIALIZED)     message += " (Not initialized)";
        else if (hr == AUDCLNT_E_ALREADY_INITIALIZED) message += " (Already initialized)";
        else if (hr == AUDCLNT_E_DEVICE_INVALIDATED)  message += " (Device disconnected/invalidated)";
        else if (hr == AUDCLNT_E_SERVICE_NOT_RUNNING) message += " (Audio service not running)";
        else if (hr == E_OUTOFMEMORY)                 message += " (Out of memory)";
        else if (hr == E_INVALIDARG)                  message += " (Invalid argument)";

        return message;
    }
};

/**
 * @brief Helper macro: check HRESULT and throw WasapiException on failure.
 */
#define WASAPI_CHECK_HR(hr, context) \
    do { \
        HRESULT __hr = (hr); \
        if (FAILED(__hr)) { \
            throw ::wasapi::WasapiException(__hr, context); \
        } \
    } while (0)

/// Returns the string name of an ErrorCode.
[[nodiscard]] inline constexpr const char* getErrorCodeName(ErrorCode code) noexcept {
    switch (code) {
        case ErrorCode::Success:              return "Success";
        case ErrorCode::DeviceNotFound:       return "DeviceNotFound";
        case ErrorCode::DeviceInUse:          return "DeviceInUse";
        case ErrorCode::DeviceDisconnected:   return "DeviceDisconnected";
        case ErrorCode::InvalidFormat:        return "InvalidFormat";
        case ErrorCode::InvalidParameter:     return "InvalidParameter";
        case ErrorCode::InitializationFailed: return "InitializationFailed";
        case ErrorCode::AllocationFailed:     return "AllocationFailed";
        case ErrorCode::NotInitialized:       return "NotInitialized";
        case ErrorCode::AlreadyInitialized:   return "AlreadyInitialized";
        case ErrorCode::Timeout:              return "Timeout";
        case ErrorCode::BufferUnderrun:       return "BufferUnderrun";
        case ErrorCode::BufferOverrun:        return "BufferOverrun";
        case ErrorCode::SessionExpired:       return "SessionExpired";
        case ErrorCode::ComError:             return "ComError";
        case ErrorCode::UnknownError:         return "UnknownError";
        default:                              return "Unknown";
    }
}

} // namespace wasapi
