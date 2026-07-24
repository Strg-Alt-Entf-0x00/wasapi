# ============================================================================
# WASAPI Examples
# ============================================================================

# Simple playback example
add_executable(example_playback
    playback_simple.cpp
)

target_link_libraries(example_playback PRIVATE wasapi)

# Device enumeration example
add_executable(example_enumerate
    enumerate_devices.cpp
)

target_link_libraries(example_enumerate PRIVATE wasapi)

# Format converter test
add_executable(test_format_converter
    test_format_converter.cpp
)

target_link_libraries(test_format_converter PRIVATE wasapi)

# Debug clamping
add_executable(debug_clamping
    debug_clamping.cpp
)

target_link_libraries(debug_clamping PRIVATE wasapi)

# Microphone recording example
add_executable(record_microphone
    record_microphone.cpp
)

target_link_libraries(record_microphone PRIVATE wasapi)

# Loopback recording example
add_executable(record_loopback
    record_loopback.cpp
)

target_link_libraries(record_loopback PRIVATE wasapi)

# Full-duplex echo test
add_executable(full_duplex_echo
    full_duplex_echo.cpp
)

target_link_libraries(full_duplex_echo PRIVATE wasapi)

# Hot-plug device monitoring
add_executable(hot_plug_demo
    hot_plug_demo.cpp
)

target_link_libraries(hot_plug_demo PRIVATE wasapi)

# Peak meter
add_executable(peak_meter
    peak_meter.cpp
)

target_link_libraries(peak_meter PRIVATE wasapi)

# Session events
add_executable(session_events
    session_events.cpp
)

target_link_libraries(session_events PRIVATE wasapi)

# Installation (optional)
install(TARGETS
        example_playback example_enumerate test_format_converter
        record_microphone record_loopback full_duplex_echo
        hot_plug_demo peak_meter session_events
    RUNTIME DESTINATION bin/examples
)
