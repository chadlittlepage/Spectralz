include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(Catch2)

add_executable(SpectralzTests
    test_main.cpp
    # Add test files here
)

target_link_libraries(SpectralzTests
    PRIVATE
        Catch2::Catch2WithMain
        juce::juce_core
        juce::juce_audio_basics
        juce::juce_dsp
)

target_include_directories(SpectralzTests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

include(CTest)
include(Catch)
catch_discover_tests(SpectralzTests)
