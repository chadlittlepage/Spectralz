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

# ONNX Runtime for ML inference (cross-platform support)
if(WIN32)
    # Windows: Look for ONNX Runtime in common locations or use ONNXRUNTIME_ROOT
    set(ONNXRUNTIME_ROOT "" CACHE PATH "ONNX Runtime root directory")

    # Search common locations if not specified
    if(NOT ONNXRUNTIME_ROOT OR NOT EXISTS "${ONNXRUNTIME_ROOT}")
        # Check for CI-provided path or common locations
        foreach(CANDIDATE_PATH
            "$ENV{ONNXRUNTIME_ROOT}"
            "${CMAKE_CURRENT_SOURCE_DIR}/onnxruntime"
            "${CMAKE_BINARY_DIR}/onnxruntime"
            "C:/onnxruntime"
            "$ENV{ProgramFiles}/onnxruntime")
            if(EXISTS "${CANDIDATE_PATH}/include/onnxruntime_cxx_api.h")
                set(ONNXRUNTIME_ROOT "${CANDIDATE_PATH}")
                break()
            endif()
        endforeach()
    endif()

    if(EXISTS "${ONNXRUNTIME_ROOT}/include/onnxruntime_cxx_api.h")
        message(STATUS "ONNX Runtime found at ${ONNXRUNTIME_ROOT}")
        set(SPECTRALZ_ENABLE_ML ON)
        set(ONNXRUNTIME_INCLUDE_DIR "${ONNXRUNTIME_ROOT}/include")
        set(ONNXRUNTIME_LIBRARY "${ONNXRUNTIME_ROOT}/lib/onnxruntime.lib")
        set(ONNXRUNTIME_DLL "${ONNXRUNTIME_ROOT}/lib/onnxruntime.dll")
    else()
        message(STATUS "ONNX Runtime not found - ML features will be disabled")
        set(SPECTRALZ_ENABLE_ML OFF)
    endif()
elseif(APPLE)
    # macOS: Homebrew installation
    set(ONNXRUNTIME_ROOT "/opt/homebrew" CACHE PATH "ONNX Runtime root directory")
    if(EXISTS "${ONNXRUNTIME_ROOT}/include/onnxruntime/onnxruntime_cxx_api.h")
        message(STATUS "ONNX Runtime found at ${ONNXRUNTIME_ROOT}")
        set(SPECTRALZ_ENABLE_ML ON)
        set(ONNXRUNTIME_INCLUDE_DIR "${ONNXRUNTIME_ROOT}/include/onnxruntime")
        set(ONNXRUNTIME_LIBRARY "${ONNXRUNTIME_ROOT}/lib/libonnxruntime.dylib")
    else()
        message(STATUS "ONNX Runtime not found - ML features will be disabled")
        set(SPECTRALZ_ENABLE_ML OFF)
    endif()
else()
    # Linux: Check common locations
    set(ONNXRUNTIME_ROOT "/usr/local" CACHE PATH "ONNX Runtime root directory")
    if(EXISTS "${ONNXRUNTIME_ROOT}/include/onnxruntime/onnxruntime_cxx_api.h")
        message(STATUS "ONNX Runtime found at ${ONNXRUNTIME_ROOT}")
        set(SPECTRALZ_ENABLE_ML ON)
        set(ONNXRUNTIME_INCLUDE_DIR "${ONNXRUNTIME_ROOT}/include/onnxruntime")
        set(ONNXRUNTIME_LIBRARY "${ONNXRUNTIME_ROOT}/lib/libonnxruntime.so")
    elseif(EXISTS "${ONNXRUNTIME_ROOT}/include/onnxruntime_cxx_api.h")
        message(STATUS "ONNX Runtime found at ${ONNXRUNTIME_ROOT}")
        set(SPECTRALZ_ENABLE_ML ON)
        set(ONNXRUNTIME_INCLUDE_DIR "${ONNXRUNTIME_ROOT}/include")
        set(ONNXRUNTIME_LIBRARY "${ONNXRUNTIME_ROOT}/lib/libonnxruntime.so")
    else()
        message(STATUS "ONNX Runtime not found - ML features will be disabled")
        set(SPECTRALZ_ENABLE_ML OFF)
    endif()
endif()

# Sentry for crash reporting (optional)
find_package(sentry QUIET)
if(sentry_FOUND)
    set(SPECTRALZ_ENABLE_SENTRY ON)
endif()

# Source files - Phase 1: Foundation
set(SPECTRALZ_SOURCES
    # Core
    src/Core/AudioEngine.cpp
    src/Core/AudioEngine.h

    # DSP
    src/DSP/FFTProcessor.cpp
    src/DSP/FFTProcessor.h

    # UI
    src/UI/SpectralzLookAndFeel.cpp
    src/UI/SpectralzLookAndFeel.h

    # UI Components
    src/UI/Components/SpectrogramView.cpp
    src/UI/Components/SpectrogramView.h
    src/UI/Components/WaveformView.cpp
    src/UI/Components/WaveformView.h
    src/UI/Components/TransportControls.cpp
    src/UI/Components/TransportControls.h
    src/UI/Components/SettingsPanel.cpp
    src/UI/Components/SettingsPanel.h
    src/UI/Components/VUMeter.cpp
    src/UI/Components/VUMeter.h

    # Plugin
    src/Plugin/PluginProcessor.cpp
    src/Plugin/PluginProcessor.h
    src/Plugin/PluginEditor.cpp
    src/Plugin/PluginEditor.h

    # Custom Standalone (provides native macOS UI)
    src/Standalone/StandaloneApp.cpp
)

# Phase 2: Spectral Editing
set(SPECTRALZ_PHASE2_SOURCES
    # Core
    src/Core/ProjectState.cpp
    src/Core/ProjectState.h
    src/Core/UndoManager.cpp
    src/Core/UndoManager.h

    # DSP
    src/DSP/SpectralEdit.cpp
    src/DSP/SpectralEdit.h
    src/DSP/SpectralLayer.cpp
    src/DSP/SpectralLayer.h
    src/DSP/SpectralEditor.cpp
    src/DSP/SpectralEditor.h
    src/DSP/PhaseVocoder.cpp
    src/DSP/PhaseVocoder.h

    # UI Components
    src/UI/Components/LayerPanel.cpp
    src/UI/Components/LayerPanel.h
    src/UI/Components/ToolPanel.cpp
    src/UI/Components/ToolPanel.h
)

# Add Phase 2 sources to main sources
list(APPEND SPECTRALZ_SOURCES ${SPECTRALZ_PHASE2_SOURCES})

# Phase 3: ML Stem Separation
if(SPECTRALZ_ENABLE_ML)
    set(SPECTRALZ_PHASE3_SOURCES
        src/ML/ONNXInference.cpp
        src/ML/ONNXInference.h
        src/ML/ModelManager.cpp
        src/ML/ModelManager.h
        src/ML/StemSeparator.cpp
        src/ML/StemSeparator.h
        src/ML/StemSeparationManager.cpp
        src/ML/StemSeparationManager.h
        src/UI/Components/StemSeparationPanel.cpp
        src/UI/Components/StemSeparationPanel.h
    )
    list(APPEND SPECTRALZ_SOURCES ${SPECTRALZ_PHASE3_SOURCES})
endif()

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
    FORMATS Standalone VST3 AU
    VST3_CATEGORIES "Fx" "Tools"
    AU_MAIN_TYPE "kAudioUnitType_Effect"
    PRODUCT_NAME "Spectralz"
)

target_sources(Spectralz PRIVATE ${SPECTRALZ_SOURCES})

target_include_directories(Spectralz PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    $<$<BOOL:${SPECTRALZ_ENABLE_ML}>:${ONNXRUNTIME_INCLUDE_DIR}>
)

target_compile_definitions(Spectralz PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    # Use custom standalone app for native macOS UI (traffic lights, no Options button)
    JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1
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
        $<$<BOOL:${SPECTRALZ_ENABLE_ML}>:${ONNXRUNTIME_LIBRARY}>
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
