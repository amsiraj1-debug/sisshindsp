cmake_minimum_required(VERSION 3.24)
project(SishhinDSP VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.16-security
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(SishhinDSP
  COMPANY_NAME SishhinDSP
  BUNDLE_ID com.sishhindsp.amp
  PLUGIN_MANUFACTURER_CODE Sshn
  PLUGIN_CODE SshA
  FORMATS VST3 Standalone
  PRODUCT_NAME "SishhinDSP"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  VST3_CATEGORIES Fx Distortion
  NEEDS_WEB_BROWSER TRUE
  NEEDS_WEBVIEW2 TRUE
  COPY_PLUGIN_AFTER_BUILD FALSE)

juce_generate_juce_header(SishhinDSP)

juce_add_binary_data(SishhinWeb
  HEADER_NAME SishhinWebData.h
  NAMESPACE SishhinWebData
  SOURCES
    dist/index.html
    dist/style.css
    dist/app.js
    dist/audio.js
    dist/native-bridge.js
    dist/pitch-worklet.js
    dist/assets/logo-cream.png
    dist/assets/logo-ink.png)

target_sources(SishhinDSP PRIVATE
  Source/PluginProcessor.h
  Source/PluginProcessor.cpp
  Source/PluginEditor.h
  Source/PluginEditor.cpp
  Source/DspEngine.h
  Source/DspEngine.cpp)

target_link_libraries(SishhinDSP PRIVATE
  SishhinWeb
  juce::juce_audio_utils
  juce::juce_dsp
  juce::juce_gui_extra
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

target_compile_definitions(SishhinDSP PRIVATE
  JUCE_WEB_BROWSER=1
  JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0
  JUCE_REPORT_APP_USAGE=0)
