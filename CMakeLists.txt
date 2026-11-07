cmake_minimum_required(VERSION 3.22)
project(StonogaFX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.4
  GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(StonogaFX
  COMPANY_NAME "Stonoga"
  BUNDLE_ID "pl.stonoga.fx"
  PLUGIN_MANUFACTURER_CODE Stng
  PLUGIN_CODE StFx
  FORMATS VST3 Standalone
  PRODUCT_NAME "Stonoga FX"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_CATEGORIES Fx Distortion)

target_sources(StonogaFX PRIVATE Source/PluginProcessor.cpp)

target_compile_definitions(StonogaFX PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0)

if(MSVC)
  target_compile_options(StonogaFX PRIVATE /utf-8)
endif()

target_link_libraries(StonogaFX
  PRIVATE juce::juce_audio_utils juce::juce_dsp
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
