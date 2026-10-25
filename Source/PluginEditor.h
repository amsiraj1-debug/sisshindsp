#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <optional>

class SishhinDSPAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                              private juce::Timer {
public:
  explicit SishhinDSPAudioProcessorEditor(SishhinDSPAudioProcessor&);
  ~SishhinDSPAudioProcessorEditor() override;
  void resized() override;
public:
  static std::optional<juce::WebBrowserComponent::Resource> getResource(const juce::String& path);
private:
  void timerCallback() override;
  void sendState();
  void sendEmbeddedLogos();
  void chooseIr();
  void sendJavascript(const juce::String& script);
  SishhinDSPAudioProcessor& processor;
  juce::WebBrowserComponent browser;
  std::unique_ptr<juce::FileChooser> chooser;
  juce::String lastSentState;
  bool pageReady=false;
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SishhinDSPAudioProcessorEditor)
};
