cmake_minimum_required(VERSION 3.22)

# Platform defaults. These must be set before project().
#   macOS:   AU + VST3 + Standalone, universal (Apple Silicon + Intel), macOS 10.13 (High Sierra) or later.
#            10.13 is the oldest macOS current Xcode can build for. Apple Silicon Macs all run macOS 11 or later.
#   iOS:     AUv3 + Standalone app (the app hosts the AUv3 extension), iOS 13+
#            configure with -G Xcode -DCMAKE_SYSTEM_NAME=iOS
#   Windows: VST3 + Standalone, x64, static MSVC runtime (no VC++ redistributable needed)
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum iOS")
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "iOS architecture")
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin") # APPLE isn't defined until project()
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary")
endif()

# Static MSVC runtime, so the plug-in loads on PCs without the Visual C++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(EQScope VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# DSP tests only (no JUCE needed): cmake -B build-tests -DEQSCOPE_TESTS_ONLY=ON
option(EQSCOPE_TESTS_ONLY "Build only the DSP unit tests" OFF)
if(EQSCOPE_TESTS_ONLY)
    add_executable(eqscope_tests tests/core_test.cpp)
    target_include_directories(eqscope_tests PRIVATE Source)
    if(MSVC)
        target_compile_options(eqscope_tests PRIVATE /utf-8 /EHsc)
    endif()
    return()
endif()

if(IOS)
    set(EQSCOPE_DEFAULT_FORMATS "AUv3;Standalone")
    set(EQSCOPE_DEFAULT_COPY OFF)
elseif(APPLE)
    set(EQSCOPE_DEFAULT_FORMATS "AU;VST3;Standalone")
    set(EQSCOPE_DEFAULT_COPY ON)
else()
    set(EQSCOPE_DEFAULT_FORMATS "VST3;Standalone")
    # Copying into C:\Program Files\Common Files\VST3 needs admin rights, so it's off on Windows.
    set(EQSCOPE_DEFAULT_COPY OFF)
endif()
set(EQSCOPE_FORMATS "${EQSCOPE_DEFAULT_FORMATS}" CACHE STRING "Plug-in formats to build")
option(EQSCOPE_COPY_AFTER_BUILD "Install the plug-in into the system plug-in folders after building" ${EQSCOPE_DEFAULT_COPY})

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.15
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

# EQScope was called "EQ Probe" before 1.0 shipped. The plug-in code, manufacturer code and bundle ID
# are kept from then, so sessions saved with EQ Probe open EQScope, and installing EQScope replaces
# EQ Probe instead of adding a second copy. Keep them the same in every future version.
juce_add_plugin(EQScope
    COMPANY_NAME "Humble Bexar Audio"
    BUNDLE_ID com.humblebexar.eqprobe
    PLUGIN_MANUFACTURER_CODE Hmbx
    PLUGIN_CODE Eqpb
    PRODUCT_NAME "EQScope"
    DESCRIPTION "Shows the frequency and phase response of an EQ"
    FORMATS ${EQSCOPE_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${EQSCOPE_COPY_AFTER_BUILD}
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Analyzer
    # iOS app settings (ignored on other platforms). The standalone app's Analyze mode
    # listens to the microphone, so iOS needs a reason string for that permission.
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "EQScope analyzes the audio coming into this device to show a frequency response."
    REQUIRES_FULL_SCREEN FALSE
    STATUS_BAR_HIDDEN TRUE
    TARGETED_DEVICE_FAMILY "1,2"
    IPHONE_SCREEN_ORIENTATIONS UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight UIInterfaceOrientationPortrait
    IPAD_SCREEN_ORIENTATIONS UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight UIInterfaceOrientationPortrait UIInterfaceOrientationPortraitUpsideDown)

target_sources(EQScope PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(EQScope PRIVATE Source)

target_compile_definitions(EQScope PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

if(MSVC)
    target_compile_options(EQScope PRIVATE /utf-8)
endif()

# Humble Bexar bear head logo for the faceplate label (Source/BexarLogo.h)
juce_add_binary_data(EQScopeBexarLogoData HEADER_NAME BexarLogoData.h NAMESPACE BexarLogoData SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/logo/humble-bexar-bear-head.png
    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/logo/humble-bexar-bear-head-cream.png)
set_target_properties(EQScopeBexarLogoData PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(EQScope PRIVATE EQScopeBexarLogoData)

target_link_libraries(EQScope
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Screenshot tool for docs/images (see tools/snapshot.cpp): cmake -B build -DEQSCOPE_SNAPSHOT=ON
option(EQSCOPE_SNAPSHOT "Also build the faceplate screenshot tool" OFF)
if(EQSCOPE_SNAPSHOT)
    juce_add_console_app(eqscope_snapshot PRODUCT_NAME "eqscope_snapshot")
    target_link_libraries(eqscope_snapshot PRIVATE EQScopeBexarLogoData)
    target_sources(eqscope_snapshot PRIVATE tools/snapshot.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_include_directories(eqscope_snapshot PRIVATE Source)
    target_compile_definitions(eqscope_snapshot PRIVATE
        JucePlugin_Name="EQScope" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(eqscope_snapshot PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
endif()
