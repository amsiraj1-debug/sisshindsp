#include "DspEngine.h"
#include <cmath>

namespace sishhin {
void Biquad::configure(Type type, float frequency, float q, float gainDb, double sampleRate) noexcept {
  const double fs=juce::jmax(8000.0, sampleRate);
  const double f=juce::jlimit(15.0,fs*0.46,static_cast<double>(frequency));
  const double w=juce::MathConstants<double>::twoPi*f/fs;
  const double co=std::cos(w), si=std::sin(w);
  const double Q=juce::jmax(0.1, static_cast<double>(q));
  const double alpha=si/(2.0*Q), A=std::pow(10.0, gainDb/40.0);
  double B0=1,B1=0,B2=0,A0=1,A1=0,A2=0;
  switch(type) {
    case Type::peak:
      B0=1+alpha*A; B1=-2*co; B2=1-alpha*A;
      A0=1+alpha/A; A1=-2*co; A2=1-alpha/A; break;
    case Type::lowPass:
      B0=(1-co)/2; B1=1-co; B2=(1-co)/2;
      A0=1+alpha; A1=-2*co; A2=1-alpha; break;
    case Type::highPass:
      B0=(1+co)/2; B1=-(1+co); B2=(1+co)/2;
      A0=1+alpha; A1=-2*co; A2=1-alpha; break;
    case Type::lowShelf: {
      const double t=2*std::sqrt(A)*alpha;
      B0=A*((A+1)-(A-1)*co+t); B1=2*A*((A-1)-(A+1)*co); B2=A*((A+1)-(A-1)*co-t);
      A0=(A+1)+(A-1)*co+t; A1=-2*((A-1)+(A+1)*co); A2=(A+1)+(A-1)*co-t; break;
    }
    case Type::highShelf: {
      const double t=2*std::sqrt(A)*alpha;
      B0=A*((A+1)+(A-1)*co+t); B1=-2*A*((A-1)+(A+1)*co); B2=A*((A+1)+(A-1)*co-t);
      A0=(A+1)-(A-1)*co+t; A1=2*((A-1)-(A+1)*co); A2=(A+1)-(A-1)*co-t; break;
    }
  }
  b0=static_cast<float>(B0/A0); b1=static_cast<float>(B1/A0); b2=static_cast<float>(B2/A0);
  a1=static_cast<float>(A1/A0); a2=static_cast<float>(A2/A0);
}

void DspEngine::prepare(double sampleRate, int maxBlock, int channelCount) {
  sr=sampleRate; channels=juce::jlimit(1,2,channelCount);
  metalHighpassCoeff=static_cast<float>(1.0-std::exp(
    -juce::MathConstants<double>::twoPi*115.0/sr));
  thickLowpassCoeff=static_cast<float>(1.0-std::exp(
    -juce::MathConstants<double>::twoPi*220.0/sr));
  delaySize=static_cast<size_t>(std::ceil(sr*2.2));
  pitchSize=static_cast<size_t>(std::ceil(sr*.18));
  for(int c=0;c<2;c++) {
    delayLine[c].assign(delaySize,0); pitchLine[c].assign(pitchSize,0);
    for(auto& f:filters[c]) f.reset();
  }
  reverb.setSampleRate(sr);
  convolution.prepare({sr,static_cast<juce::uint32>(maxBlock),static_cast<juce::uint32>(channels)});
  reset();
}
void DspEngine::reset() {
  writePos=0; pitchWrite=0; pitchPhase=0;
  for(int c=0;c<2;c++) {
    gateEnvelope[c]=0;
    metalLowState[c]=0;
    thickLowState[c]=0;
    std::fill(delayLine[c].begin(),delayLine[c].end(),0);
    std::fill(pitchLine[c].begin(),pitchLine[c].end(),0);
    for(auto& f:filters[c]) f.reset();
  }
  reverb.reset(); convolution.reset();
}
void DspEngine::loadIr(const juce::File& file) {
  if (!file.existsAsFile()) return;
  convolution.loadImpulseResponse(file,juce::dsp::Convolution::Stereo::yes,
                                  juce::dsp::Convolution::Trim::yes,0,
                                  juce::dsp::Convolution::Normalise::yes);
  useIr.store(true);
}
void DspEngine::updateFilters(const Settings& s) {
  const int cabinet=juce::jlimit(0,2,static_cast<int>(s.cab));
  const int voice=juce::jlimit(0,2,static_cast<int>(s.amp));
  static constexpr float cabHp[]={65,85,55},cabLp[]={7400,5900,4800},cabBody[]={140,115,95};
  static constexpr float hz[]={65,125,250,500,1000,2000,4000,8000,16000};
  for(int c=0;c<channels;c++) {
    auto& f=filters[c];
    // Porcelain: brilliant, open cleans. Katana: tight lows and upper-mid bite.
    // Monolith: substantial bass/low mids, rolled-off top-end for weight.
    const float bassTrim=voice==0?0.0f:voice==1?-2.5f:5.0f;
    const float midTrim=voice==0?-.5f:voice==1?3.0f:2.6f;
    const float trebleTrim=voice==0?2.0f:voice==1?1.5f:-4.0f;
    const float presenceTrim=voice==0?1.4f:voice==1?3.0f:-2.0f;
    f[0].configure(Biquad::Type::lowShelf,voice==2?155.0f:170.0f,.707f,
                   (s.bass-5)*1.8f+bassTrim,sr);
    f[1].configure(Biquad::Type::peak,voice==2?350.0f:voice==1?1050.0f:800.0f,.8f,
                   (s.mid-5)*1.8f+midTrim,sr);
    f[2].configure(Biquad::Type::highShelf,voice==0?3000.0f:2600.0f,.707f,
                   (s.treble-5)*1.8f+trebleTrim,sr);
    f[3].configure(Biquad::Type::peak,voice==1?3700.0f:voice==2?2300.0f:5100.0f,.8f,
                   (s.presence-5)*1.4f+presenceTrim,sr);
    f[4].configure(Biquad::Type::highPass,
                   cabHp[cabinet]+(voice==1?25.0f:0.0f),.707f,0,sr);
    f[5].configure(Biquad::Type::lowPass,
                   juce::jmin(cabLp[cabinet],voice==2?4900.0f:voice==1?6500.0f:9000.0f),
                   .707f,0,sr);
    f[6].configure(Biquad::Type::peak,voice==2?135.0f:cabBody[cabinet],1.1f,
                   voice==2?4.5f:voice==1?1.3f:2.0f,sr);
    for(int i=0;i<9;i++) f[7+i].configure(Biquad::Type::peak,hz[i],1.2f,s.eq[i],sr);
    f[16].configure(Biquad::Type::highPass,s.hp,.707f,0,sr);
    f[17].configure(Biquad::Type::lowPass,s.lp,.707f,0,sr);
  }
}
float DspEngine::interp(const std::vector<float>& ring,float pos) const noexcept {
  const auto n=static_cast<int>(ring.size());
  if(n<2)return 0;
  while(pos<0) pos+=n;
  while(pos>=n) pos-=n;
  const auto i=static_cast<int>(pos), j=(i+1)%n;
  return ring[static_cast<size_t>(i)] + (ring[static_cast<size_t>(j)]-ring[static_cast<size_t>(i)])*(pos-i);
}
float DspEngine::pitchSample(int ch,float input,float semi) noexcept {
  auto& ring=pitchLine[ch];
  ring[static_cast<size_t>(pitchWrite)]=input;
  if(std::abs(semi)<.005f) return input;
  const float ratio=std::pow(2.0f,semi/12.0f);
  const float minDelay=static_cast<float>(sr*.017);
  const float maxDelay=static_cast<float>(sr*.085);
  const float span=maxDelay-minDelay;
  const float phase=static_cast<float>(pitchPhase);
  const auto read=[&](float p) {
    const float delay=ratio>=1.0f ? maxDelay-p*span : minDelay+p*span;
    return interp(ring,static_cast<float>(pitchWrite)-delay);
  };
  const float p2=std::fmod(phase+.5f,1.0f);
  const float w=.5f-.5f*std::cos(juce::MathConstants<float>::twoPi*phase);
  return w*read(phase)+(1-w)*read(p2);
}
void DspEngine::process(juce::AudioBuffer<float>& b,const Settings& s) {
  const int ch=juce::jmin(channels,b.getNumChannels());
  const int n=b.getNumSamples();
  if(n<=0 || ch<=0) return;
  updateFilters(s);
  const float inputGain=db(s.input),outputGain=db(s.output);
  const auto voice=juce::jlimit(0,2,static_cast<int>(s.amp));
  // Each amplifier has a separate transfer function, not merely a drive scalar.
  const float preBoost=(s.weightOn>.5f?1+s.boost*.28f:1);
  const float edge=(s.edgeOn>.5f?1+s.drive*.7f:0);
  const float edgeLevel=s.edgeOn>.5f?.35f+s.level*.14f:1;
  const float toneLowpass=juce::jmap(s.tone,0.f,10.f,1500.f,10000.f);
  const float gateThreshold=db(s.gate);
  const float delaySamples=juce::jlimit(1.0f,static_cast<float>(delaySize-2),s.delayTime*.001f*static_cast<float>(sr));
  const float doubleSamples=juce::jlimit(1.0f,static_cast<float>(delaySize-2),s.doubler*.001f*static_cast<float>(sr));
  float rms=0;
  for(int i=0;i<n;i++) {
    for(int c=0;c<ch;c++) {
      const float dry=b.getSample(c,i)*inputGain;
      const float mag=std::abs(dry);
      const float coef=mag>gateEnvelope[c]? .018f:.00016f;
      gateEnvelope[c]+=coef*(mag-gateEnvelope[c]);
      const float gated=(s.gateOn>.5f && gateEnvelope[c]<gateThreshold)?dry*.06f:dry;
      float x=pitchSample(c,gated,s.transpose);
      if(s.bloomOn>.5f) x=std::tanh(x*(1.2f+s.bloom*.27f))*.78f;
      x*=preBoost;
      if(s.edgeOn>.5f) {
        const float tone=juce::jlimit(.2f,1.f,toneLowpass/10000.f);
        x=std::tanh(x*edge)*edgeLevel*tone;
      }
      // Keep the low-frequency tracking filters warm as models change.
      metalLowState[c]+=metalHighpassCoeff*(x-metalLowState[c]);
      thickLowState[c]+=thickLowpassCoeff*(x-thickLowState[c]);
      if(voice==0) {
        // PORCELAIN: preserve pick attack and chord definition. Mild warm
        // limiting only on high peaks, rather than clipping every note.
        const float cleanDrive=.70f+s.gain*.23f;
        const float clean=x*cleanDrive;
        x=(clean*.88f+std::tanh(clean*1.12f)*.12f)*(s.master*.27f);
      } else if(voice==1) {
        // KATANA: high-pass *before* high gain, then two non-linear preamp
        // stages, with an upper-mid emphasis in the post-stage tone stack.
        const float tight=x-.87f*metalLowState[c];
        const float first=std::tanh(tight*(2.1f+s.gain*.72f));
        const float second=std::tanh(first*(2.8f+s.gain*.39f)+tight*.16f);
        x=(second*.95f+tight*.05f)*(s.master*.17f);
      } else {
        // MONOLITH: wide, bass-heavy preamp, slow low-mid bloom and soft
        // saturating tube-style compression. Denser but less sharp than metal.
        const float fat=x+.63f*thickLowState[c];
        const float warm=std::tanh(fat*(1.20f+s.gain*.48f));
        x=(warm*.84f+fat*.16f)*(s.master*.22f);
      }
      for(int f=0;f<4;f++)x=filters[c][f].tick(x);
      b.setSample(c,i,x);
    }
    pitchWrite=(pitchWrite+1)%static_cast<int>(pitchSize);
    if(std::abs(s.transpose)>.005f) {
      const float ratio=std::pow(2.0f,s.transpose/12.0f);
      pitchPhase+=std::abs(ratio-1.0f)/juce::jmax(1.0, sr*.068);
      if(pitchPhase>=1)pitchPhase-=std::floor(pitchPhase);
    }
  }
  if(useIr.load(std::memory_order_relaxed)) {
    juce::dsp::AudioBlock<float> block(b);
    juce::dsp::ProcessContextReplacing<float> context(block);
    convolution.process(context);
  }
  for(int i=0;i<n;i++) {
    for(int c=0;c<ch;c++) {
      float x=b.getSample(c,i);
      for(int f=4;f<filterCount;f++)x=filters[c][f].tick(x);
      const float A=db(s.micLevelA)*(1.0f-s.distA*.35f)*.64f;
      const float B=db(s.micLevelB)*(1.0f-s.distB*.35f)*.64f;
      const float pA=c==0?std::sqrt((1-s.panA)*.5f):std::sqrt((1+s.panA)*.5f);
      const float pB=c==0?std::sqrt((1-s.panB)*.5f):std::sqrt((1+s.panB)*.5f);
      const float micTone=1.0f+(s.micA==2?.14f:s.micA==1?-.09f:0.f);
      const float micBTone=1.0f+(s.micB==2?.14f:s.micB==1?-.09f:0.f);
      x *= (A*pA*micTone+B*pB*micBTone);
      auto& d=delayLine[c];
      const float delayed=interp(d,static_cast<float>(writePos)-delaySamples);
      const float doubled=interp(d,static_cast<float>(writePos)-doubleSamples);
      const float feedback=juce::jlimit(0.f,.8f,s.feedback*.01f);
      d[static_cast<size_t>(writePos)]=x+(s.delayOn>.5f?delayed*feedback:0);
      if(s.delayOn>.5f)x+=delayed*(s.delayMix*.01f);
      if(s.doublerOn>.5f)x+=doubled*.3f*(c==0?.6f:1.0f);
      b.setSample(c,i,x);
    }
    writePos=(writePos+1)%static_cast<int>(delaySize);
  }
  juce::Reverb::Parameters r;
  r.roomSize=juce::jlimit(0.f,1.f,(s.decay-.3f)/7.f);
  r.damping=.4f; r.width=1.f;
  r.wetLevel=s.reverbOn>.5f ? s.reverbMix*.0033f:0.f;
  r.dryLevel=1.0f; // Keep the amp signal intact when reverb is enabled.
  reverb.setParameters(r);
  if(ch==2) reverb.processStereo(b.getWritePointer(0),b.getWritePointer(1),n);
  else reverb.processMono(b.getWritePointer(0),n);
  for(int c=0;c<ch;c++){
    auto* d=b.getWritePointer(c);
    for(int i=0;i<n;i++){
      d[i]=std::tanh(d[i]*outputGain);
      rms+=d[i]*d[i];
    }
  }
  juce::ignoreUnused(rms);
}
} // namespace sishhin
