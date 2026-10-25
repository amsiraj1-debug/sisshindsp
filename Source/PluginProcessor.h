#pragma once
#include <JuceHeader.h>
#include "DspEngine.h"
#include <array>
#include <atomic>

class SishhinDSPAudioProcessor final : public juce::AudioProcessor {
public:
  SishhinDSPAudioProcessor();
  ~SishhinDSPAudioProcessor() override = default;
  void prepareToPlay(double,int) override;
  void releaseResources() override;
  bool isBusesLayoutSupported(const BusesLayout&) const override;
  void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
  juce::AudioProcessorEditor* createEditor() override;
  bool hasEditor() const override { return true; }
  const juce::String getName() const override { return "SishhinDSP"; }
  bool acceptsMidi() const override { return true; }
  bool producesMidi() const override { return false; }
  bool isMidiEffect() const override { return false; }
  double getTailLengthSeconds() const override { return 8.0; }
  int getNumPrograms() override { return 1; }
  int getCurrentProgram() override { return 0; }
  void setCurrentProgram(int) override {}
  const juce::String getProgramName(int) override { return "Current"; }
  void changeProgramName(int,const juce::String&) override {}
  void getStateInformation(juce::MemoryBlock&) override;
  void setStateInformation(const void*,int) override;

  juce::AudioProcessorValueTreeState apvts;
  static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
  sishhin::Settings collectSettings() const noexcept;
  void setFromJson(const juce::var&);
  juce::String currentJson() const;
  void loadIR(const juce::File&);
  void removeIR();
  juce::String getIrName() const { return irFile.getFileName(); }
  float getLevel() const {return level.load();}
  float getPitchHz() const;
  double getCurrentSampleRate() const { return getSampleRate(); }

  struct MidiEvent {int status=0, number=0, value=0;};
  bool takeMidi(MidiEvent&);
private:
  sishhin::DspEngine dsp;
  juce::File irFile;
  std::atomic<float> level{0};
  std::array<std::atomic<float>,4096> tunerRing{};
  std::atomic<unsigned int> tunerIndex{0};
  juce::AbstractFifo midiFifo{128};
  std::array<MidiEvent,128> midiQueue{};
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SishhinDSPAudioProcessor)
};
