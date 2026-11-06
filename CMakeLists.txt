cmake_minimum_required(VERSION 3.22)

# IFMY DIAMOND PERFECT — incrémenter la version à CHAQUE release (Logic re-scanne le plugin)
project(IFMYDiamondPerfect VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "" FORCE)

option(DIAMOND_BUILD_PLUGINS "Construire les plugins (JUCE)" ON)

# --- Tests DSP (C++ pur, sans JUCE) ------------------------------------------
enable_testing()
add_executable(DiamondTests tests/DspTests.cpp)
add_test(NAME DiamondDSP COMMAND DiamondTests)

if(NOT DIAMOND_BUILD_PLUGINS)
    return()
endif()

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

set(DIAMOND_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PresetManager.cpp)

# ⚠️ Codes FIXES : ne jamais les changer après la première version
#    Fabricant : Ifmy (partagé avec SALOMOH / IFMY VOX)
#    Effet audio : Idmd   |   Version MIDI : Idmm
function(diamond_plugin target productName pluginCode auType isMidi)
    juce_add_plugin(${target}
        COMPANY_NAME "IFMY"
        COMPANY_WEBSITE "https://github.com/simonperiot-crypto/ifmy-diamond-perfect"
        BUNDLE_ID "com.ifmy.${pluginCode}"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT ${isMidi}
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        PLUGIN_MANUFACTURER_CODE Ifmy
        PLUGIN_CODE ${pluginCode}
        AU_MAIN_TYPE ${auType}
        VST3_CATEGORIES Fx Pitch Shift
        FORMATS AU VST3
        PRODUCT_NAME "${productName}")

    target_sources(${target} PRIVATE ${DIAMOND_SOURCES})

    if(isMidi)
        set(midiFlag 1)
    else()
        set(midiFlag 0)
    endif()

    target_compile_definitions(${target} PUBLIC
        DIAMOND_MIDI=${midiFlag}
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

    target_link_libraries(${target} PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
endfunction()

diamond_plugin(DiamondPerfect     "IFMY Diamond Perfect"      Idmd kAudioUnitType_Effect      FALSE)
diamond_plugin(DiamondPerfectMIDI "IFMY Diamond Perfect MIDI" Idmm kAudioUnitType_MusicEffect TRUE)

# --- Outil de dev (désactivé par défaut) : captures PNG de l'interface -----------
option(DIAMOND_SNAPSHOT "Construire l'outil de captures d'interface" OFF)
if(DIAMOND_SNAPSHOT)
    juce_add_console_app(DiamondSnapshot PRODUCT_NAME "DiamondSnapshot")
    target_sources(DiamondSnapshot PRIVATE tools/Snapshot.cpp)
    target_link_libraries(DiamondSnapshot PRIVATE DiamondPerfect)
    target_include_directories(DiamondSnapshot PRIVATE $<TARGET_PROPERTY:DiamondPerfect,INCLUDE_DIRECTORIES>)
    target_compile_definitions(DiamondSnapshot PRIVATE $<TARGET_PROPERTY:DiamondPerfect,COMPILE_DEFINITIONS>)
endif()
