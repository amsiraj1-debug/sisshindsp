#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace sishhin {

struct Settings {
  float amp=0, input=0, output=-12, gate=-60, gateOn=1, transpose=0, mode=0;
  float doubler=12, doublerOn=0, gain=3, bass=5, mid=5, treble=6, presence=5, master=5;
  float bloomOn=0, bloom=3, edgeOn=0, drive=2, tone=5, level=5, weightOn=0, boost=3;
  float cab=0, micA=0, micB=1, posA=.45f, posB=.6f, distA=.1f, distB=.25f;
  float micLevelA=0, micLevelB=-6, panA=-.3f, panB=.3f;
  std::array<float,9> eq{};
  float hp=60, lp=12000, delayOn=0, delayMix=18, delayTime=380, feedback=30;
  float reverbOn=1, reverbMix=14, decay=2.5f;
};

class Biquad {
public:
  enum class Type { lowShelf, highShelf, peak, lowPass, highPass };
  void configure(Type type, float frequency, float q, float gainDb, double sampleRate) noexcept;
  float tick(float x) noexcept {
    const float y = b0*x + z1;
    z1 = b1*x - a1*y + z2;
    z2 = b2*x - a2*y;
    return std::isfinite(y) ? y : 0.0f;
  }
  void reset() noexcept { z1=z2=0; }
private:
  float b0=1,b1=0,b2=0,a1=0,a2=0,z1=0,z2=0;
};

class DspEngine {
public:
  void prepare(double sampleRate, int maxBlock, int channelCount);
  void reset();
  void process(juce::AudioBuffer<float>&, const Settings&);
  void loadIr(const juce::File&);
  void clearIr() noexcept { useIr.store(false); }
  bool isIrActive() const noexcept { return useIr.load(); }
private:
  static constexpr int filterCount=18;
  void updateFilters(const Settings& s);
  float pitchSample(int ch, float input, float semi) noexcept;
  float interp(const std::vector<float>& ring, float pos) const noexcept;
  static float db(float d) noexcept { return std::pow(10.0f,d*0.05f); }
  double sr=44100;
  int channels=2, writePos=0, pitchWrite=0;
  size_t delaySize=88200, pitchSize=8192;
  float gateEnvelope[2]{};
  std::array<std::array<Biquad,filterCount>,2> filters;
  std::array<std::vector<float>,2> delayLine, pitchLine;
  double pitchPhase=0;
  juce::Reverb reverb;
  juce::dsp::Convolution convolution;
  std::atomic<bool> useIr{false};
};
} // namespace sishhin
