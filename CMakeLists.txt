cmake_minimum_required(VERSION 3.22)

project(Spectralz VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(SPECTRALZ_BUILD_STANDALONE "Build standalone application" ON)
option(SPECTRALZ_BUILD_VST3 "Build VST3 plugin" ON)
option(SPECTRALZ_BUILD_AU "Build Audio Unit plugin" ON)
option(SPECTRALZ_BUILD_ARA "Build with ARA support" OFF)
option(SPECTRALZ_BUILD_TESTS "Build unit tests" ON)

# Find JUCE - adjust path as needed
set(JUCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE" CACHE PATH "Path to JUCE")
add_subdirectory(${JUCE_PATH} ${CMAKE_BINARY_DIR}/JUCE EXCLUDE_FROM_ALL)

# ONNX Runtime for ML inference
find_package(onnxruntime QUIET)
if(NOT onnxruntime_FOUND)
    message(STATUS "ONNX Runtime not found - ML features will be disabled")
    set(SPECTRALZ_ENABLE_ML OFF)
else()
    set(SPECTRALZ_ENABLE_ML ON)
endif()

# Sentry for crash reporting (optional)
find_package(sentry QUIET)
if(sentry_FOUND)
    set(SPECTRALZ_ENABLE_SENTRY ON)
endif()

# Source files
set(SPECTRALZ_SOURCES
    src/Core/SpectralProcessor.cpp
    src/Core/SpectralProcessor.h
    src/Core/AudioEngine.cpp
    src/Core/AudioEngine.h
    src/Core/ProjectState.cpp
    src/Core/ProjectState.h

    src/DSP/FFTProcessor.cpp
    src/DSP/FFTProcessor.h
    src/DSP/SpectralEditor.cpp
    src/DSP/SpectralEditor.h
    src/DSP/StemSeparator.cpp
    src/DSP/StemSeparator.h

    src/ML/ONNXInference.cpp
    src/ML/ONNXInference.h
    src/ML/ModelManager.cpp
    src/ML/ModelManager.h

    src/UI/Components/SpectrogramView.cpp
    src/UI/Components/SpectrogramView.h
    src/UI/Components/WaveformView.cpp
    src/UI/Components/WaveformView.h
    src/UI/Components/LayerPanel.cpp
    src/UI/Components/LayerPanel.h
    src/UI/Components/ToolPanel.cpp
    src/UI/Components/ToolPanel.h

    src/UI/Editors/MainEditor.cpp
    src/UI/Editors/MainEditor.h

    src/Plugin/PluginProcessor.cpp
    src/Plugin/PluginProcessor.h
    src/Plugin/PluginEditor.cpp
    src/Plugin/PluginEditor.h
)

# JUCE Plugin/Standalone
juce_add_plugin(Spectralz
    COMPANY_NAME "SpectralzAudio"
    BUNDLE_ID "com.spectralzaudio.spectralz"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Sptz
    PLUGIN_CODE Spcz
    FORMATS
        $<$<BOOL:${SPECTRALZ_BUILD_STANDALONE}>:Standalone>
        $<$<BOOL:${SPECTRALZ_BUILD_VST3}>:VST3>
        $<$<BOOL:${SPECTRALZ_BUILD_AU}>:AU>
    VST3_CATEGORIES "Fx" "Tools"
    AU_MAIN_TYPE "kAudioUnitType_Effect"
    PRODUCT_NAME "Spectralz"
)

target_sources(Spectralz PRIVATE ${SPECTRALZ_SOURCES})

target_include_directories(Spectralz PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(Spectralz PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    $<$<BOOL:${SPECTRALZ_ENABLE_ML}>:SPECTRALZ_ENABLE_ML=1>
    $<$<BOOL:${SPECTRALZ_ENABLE_SENTRY}>:SPECTRALZ_ENABLE_SENTRY=1>
)

target_link_libraries(Spectralz
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_opengl
        $<$<BOOL:${onnxruntime_FOUND}>:onnxruntime::onnxruntime>
        $<$<BOOL:${sentry_FOUND}>:sentry::sentry>
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Tests
if(SPECTRALZ_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Install rules
install(TARGETS Spectralz
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
