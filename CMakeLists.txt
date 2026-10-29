cmake_minimum_required(VERSION 3.22)
project(MixSense VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
elseif(EXISTS "/home/shams/Desktop/vst/JUCE/CMakeLists.txt")
    add_subdirectory("/home/shams/Desktop/vst/JUCE" "${CMAKE_BINARY_DIR}/JUCE")
else()
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.6
    )
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(MixSense
    COMPANY_NAME "MixSenseDSP"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    PLUGIN_NAME "MixSense Mixing Assistant"
    PRODUCT_NAME "MixSense"
    FORMATS VST3 Standalone
    PLUGIN_CODE MxAs
    PLUGIN_MANUFACTURER_CODE MxSn
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(MixSense
    PRIVATE
        Source/PluginProcessor.h
        Source/PluginProcessor.cpp
        Source/Analysis/GenreProfiles.h
        Source/Analysis/AudioAnalyzer.h
        Source/Analysis/AudioAnalyzer.cpp
        Source/Analysis/DiagnosticEngine.h
        Source/Analysis/DiagnosticEngine.cpp
        Source/DSP/CorrectiveDSP.h
        Source/DSP/CorrectiveDSP.cpp
        Source/UI/SpectrumComponent.h
        Source/UI/SpectrumComponent.cpp
        Source/UI/AssistantFeedComponent.h
        Source/UI/AssistantFeedComponent.cpp
        Source/UI/MeterStripComponent.h
        Source/UI/MeterStripComponent.cpp
        Source/UI/PluginEditor.h
        Source/UI/PluginEditor.cpp
)

target_compile_definitions(MixSense
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(MixSense
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# Automated Headless Test Target
add_executable(MixSenseTests
    Tests/MixSenseTests.cpp
    Source/Analysis/AudioAnalyzer.cpp
    Source/Analysis/DiagnosticEngine.cpp
    Source/DSP/CorrectiveDSP.cpp
)

target_compile_definitions(MixSenseTests
    PUBLIC
        JUCE_STANDALONE_APPLICATION=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

target_link_libraries(MixSenseTests
    PRIVATE
        juce::juce_audio_basics
        juce::juce_dsp
        juce::juce_core
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

