#pragma once
/***************************************************************************
 * WASAPI - Windows Audio Library
 * 
 * COM Initialization RAII Wrapper
 * 
 * Ensures proper COM initialization/cleanup on each thread
 * 
 * License: MIT
 * Author: AI-Projects Team
 * Date: 2026-06-08
 ***************************************************************************/

#include <windows.h>
#include <wasapi/error.h>

namespace wasapi {
namespace internal {

/**
 * @brief RAII wrapper for COM initialization
 * 
 * Automatically initializes COM on construction and uninitializes on destruction.
 * Uses COINIT_MULTITHREADED for compatibility with audio threads.
 * 
 * Thread-Safety: Must be created on each thread that uses COM interfaces.
 * 
 * Example:
 * @code
 * void audioThreadFunction() {
 *     ComInitializer com;  // Initialize COM for this thread
 *     // ... use COM interfaces ...
 * }  // COM automatically uninitialized
 * @endcode
 */
class ComInitializer {
public:
    /**
     * @brief Constructor - Initializes COM
     * 
     * @throws WasapiException if COM initialization fails
     * 
     * Note: If COM is already initialized on this thread with a different
     * concurrency model, this is not fatal. The existing initialization
     * will be used.
     */
    ComInitializer() {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        
        if (SUCCEEDED(hr)) {
            // Successfully initialized COM on this thread
            comInitialized_ = true;
        }
        else if (hr == RPC_E_CHANGED_MODE) {
            // COM already initialized with different mode - acceptable
            // We didn't initialize it, so don't uninitialize it
            comInitialized_ = false;
        }
        else if (hr == S_FALSE) {
            // COM already initialized with same mode - acceptable
            // We didn't initialize it, so don't uninitialize it
            comInitialized_ = false;
        }
        else {
            // Actual error
            throw WasapiException(hr, "COM initialization failed");
        }
    }
    
    /**
     * @brief Destructor - Uninitializes COM if we initialized it
     */
    ~ComInitializer() noexcept {
        if (comInitialized_) {
            CoUninitialize();
        }
    }
    
    // Non-copyable, non-movable (tied to thread)
    ComInitializer(const ComInitializer&) = delete;
    ComInitializer& operator=(const ComInitializer&) = delete;
    ComInitializer(ComInitializer&&) = delete;
    ComInitializer& operator=(ComInitializer&&) = delete;
    
    /**
     * @brief Check if this instance initialized COM
     * @return true if we initialized COM (and will uninitialize it)
     */
    [[nodiscard]] bool didInitialize() const noexcept {
        return comInitialized_;
    }

private:
    bool comInitialized_ = false;
};

} // namespace internal
} // namespace wasapi
