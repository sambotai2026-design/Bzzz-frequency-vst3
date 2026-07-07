cmake_minimum_required(VERSION 3.22)
project(BzzzFrequency VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE : soit le dossier ../JUCE (repo cloné), soit défini via -DJUCE_DIR
if(NOT DEFINED JUCE_DIR)
    set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE")
endif()
add_subdirectory(${JUCE_DIR} JUCE)

juce_add_plugin(BzzzFrequency
    COMPANY_NAME "Paye Ta Ruche"
    BUNDLE_ID com.payetaruche.bzzzfrequency
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Ptrc
    PLUGIN_CODE Bzzf
    FORMATS VST3
    PRODUCT_NAME "BZzz Frequency")

target_sources(BzzzFrequency PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(BzzzFrequency PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(BzzzFrequency PRIVATE
    juce::juce_audio_utils
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
