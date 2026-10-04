cmake_minimum_required(VERSION 3.22)
project(LowLinkScope VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(LOWLINK_BUILD_CLAP "Build CLAP format too" ON)
option(LOWLINK_BUILD_TESTS "Build analysis unit tests" OFF)

include(FetchContent)

# JUCE: use a local checkout if JUCE_PATH is given, otherwise download it.
if(DEFINED JUCE_PATH)
    add_subdirectory(${JUCE_PATH} JUCE)
else()
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.10
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(LowLinkScope
    COMPANY_NAME "Ota"
    PRODUCT_NAME "Low Link Scope"
    BUNDLE_ID "cz.ota.lowlinkscope"
    PLUGIN_MANUFACTURER_CODE Otah
    PLUGIN_CODE Llsc
    FORMATS VST3 Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Analyzer)

target_sources(LowLinkScope PRIVATE
    Source/LinkBus.cpp
    Source/Analysis.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ScopeView.cpp)

target_include_directories(LowLinkScope PRIVATE Source)

target_compile_definitions(LowLinkScope PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(LowLinkScope
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(UNIX AND NOT APPLE)
    target_link_libraries(LowLinkScope PRIVATE rt)
endif()

if(LOWLINK_BUILD_CLAP)
    if(DEFINED CLAP_JUCE_PATH)
        add_subdirectory(${CLAP_JUCE_PATH} clap-juce-extensions EXCLUDE_FROM_ALL)
    else()
        FetchContent_Declare(clap-juce-extensions
            GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
            GIT_TAG main
            GIT_SHALLOW ON
            GIT_SUBMODULES_RECURSE ON)
        FetchContent_MakeAvailable(clap-juce-extensions)
    endif()
    clap_juce_extensions_plugin(TARGET LowLinkScope
        CLAP_ID "cz.ota.lowlinkscope"
        CLAP_FEATURES audio-effect analyzer utility)
endif()

if(LOWLINK_BUILD_TESTS)
    add_executable(analysis_tests Tests/AnalysisTests.cpp Source/Analysis.cpp Source/LinkBus.cpp)
    target_include_directories(analysis_tests PRIVATE Source)
    if(UNIX AND NOT APPLE)
        target_link_libraries(analysis_tests PRIVATE rt pthread)
    endif()
endif()
