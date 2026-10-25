#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace {
using Layout=juce::AudioProcessorValueTreeState::ParameterLayout;
void f(Layout& l,const char* id,float min,float max,float step,float def){
  l.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id,1},id,
      juce::NormalisableRange<float>{min,max,step},def));
}
void b(Layout& l,const char* id,bool def){
  l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{id,1},id,def));
}
void choice(Layout& l,const char* id,juce::StringArray names,int initial){
  l.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{id,1},id,names,initial));
}
}

Layout SishhinDSPAudioProcessor::makeLayout(){
  Layout l;
  choice(l,"amp",{"Porcelain","Katana","Monolith"},0);
  f(l,"input",-24,24,.1f,0); f(l,"output",-36,6,.1f,-12);
  f(l,"gate",-80,-20,1,-60); b(l,"gateOn",true);
  f(l,"transpose",-12,12,1,0); choice(l,"mode",{"mono","stereo"},0);
  f(l,"doubler",5,30,.1f,12); b(l,"doublerOn",false);
  f(l,"gain",0,10,.1f,3); f(l,"bass",0,10,.1f,5);
  f(l,"mid",0,10,.1f,5);f(l,"treble",0,10,.1f,6);
  f(l,"presence",0,10,.1f,5);f(l,"master",0,10,.1f,5);
  b(l,"bloomOn",false); f(l,"bloom",0,10,.1f,3);
  b(l,"edgeOn",false);f(l,"drive",0,10,.1f,2);
  f(l,"tone",0,10,.1f,5);f(l,"level",0,10,.1f,5);
  b(l,"weightOn",false);f(l,"boost",0,10,.1f,3);
  choice(l,"cab",{"1x12 Open","4x12 Modern","2x12 Oversized"},0);
  choice(l,"micA",{"Dynamic","Ribbon","Condenser"},0);
  choice(l,"micB",{"Dynamic","Ribbon","Condenser"},1);
  f(l,"posA",0,1,.01f,.45f); f(l,"posB",0,1,.01f,.6f);
  f(l,"distA",0,1,.01f,.1f); f(l,"distB",0,1,.01f,.25f);
  f(l,"micLevelA",-24,6,.1f,0);f(l,"micLevelB",-24,6,.1f,-6);
  f(l,"panA",-1,1,.01f,-.3f); f(l,"panB",-1,1,.01f,.3f);
  for(int i=0;i<9;++i) f(l,("eq"+std::to_string(i)).c_str(),-12,12,.5f,0);
  f(l,"hp",20,300,1,60);f(l,"lp",3000,20000,10,12000);
  b(l,"delayOn",false);f(l,"delayMix",0,60,1,18);
  f(l,"delayTime",60,1000,1,380);f(l,"feedback",0,80,1,30);
  b(l,"reverbOn",true);f(l,"reverbMix",0,60,1,14);
  f(l,"decay",.3f,7,.1f,2.5f);
  return l;
}

SishhinDSPAudioProcessor::SishhinDSPAudioProcessor()
  : AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true)
                                    .withOutput("Output",juce::AudioChannelSet::stereo(),true)),
    apvts(*this,nullptr,"PARAMETERS",makeLayout()) {}

void SishhinDSPAudioProcessor::prepareToPlay(double sampleRate,int maxBlock){
  dsp.prepare(sampleRate,maxBlock,getTotalNumOutputChannels());
  level.store(0); tunerIndex.store(0);
}
void SishhinDSPAudioProcessor::releaseResources(){ dsp.reset(); }
bool SishhinDSPAudioProcessor::isBusesLayoutSupported(const BusesLayout& layout) const {
  const auto in=layout.getMainInputChannelSet(),out=layout.getMainOutputChannelSet();
  return (in==juce::AudioChannelSet::mono()||in==juce::AudioChannelSet::stereo()) && in==out;
}
sishhin::Settings SishhinDSPAudioProcessor::collectSettings() const noexcept {
  sishhin::Settings s;
  const auto raw=[this](const char* key)->float {
    auto* p=apvts.getRawParameterValue(key);
    return p!=nullptr?p->load(std::memory_order_relaxed):0.f;
  };
  #define RD(x) s.x=raw(#x)
  RD(amp);RD(input);RD(output);RD(gate);RD(gateOn);RD(transpose);RD(mode);
  RD(doubler);RD(doublerOn);RD(gain);RD(bass);RD(mid);RD(treble);RD(presence);RD(master);
  RD(bloomOn);RD(bloom);RD(edgeOn);RD(drive);RD(tone);RD(level);RD(weightOn);RD(boost);
  RD(cab);RD(micA);RD(micB);RD(posA);RD(posB);RD(distA);RD(distB);
  RD(micLevelA);RD(micLevelB);RD(panA);RD(panB);
  RD(hp);RD(lp);RD(delayOn);RD(delayMix);RD(delayTime);RD(feedback);
  RD(reverbOn);RD(reverbMix);RD(decay);
  #undef RD
  for(int i=0;i<9;i++)s.eq[static_cast<size_t>(i)]=raw(("eq"+std::to_string(i)).c_str());
  return s;
}
void SishhinDSPAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer& midi){
  juce::ScopedNoDenormals noDenormals;
  const int inputChannels=getTotalNumInputChannels();
  for(int ch=inputChannels;ch<buffer.getNumChannels();ch++)buffer.clear(ch,0,buffer.getNumSamples());
  auto w=tunerIndex.load(std::memory_order_relaxed);
  if(inputChannels>0){
    for(int i=0;i<buffer.getNumSamples();i++) {
      tunerRing[static_cast<size_t>(w%tunerRing.size())].store(buffer.getSample(0,i),std::memory_order_relaxed);
      ++w;
    }
    tunerIndex.store(w,std::memory_order_release);
  }
  for(const auto meta:midi){
    const auto message=meta.getMessage();
    if(!message.isController()&&!message.isProgramChange())continue;
    const int slot=midiFifo.getFreeSpace();
    if(slot<=0)continue;
    int start1=0, size1=0, start2=0, size2=0;
    midiFifo.prepareToWrite(1,start1,size1,start2,size2);
    if(size1>0){
      midiQueue[static_cast<size_t>(start1)]={message.getRawData()[0],
        message.isController()?message.getControllerNumber():message.getProgramChangeNumber(),
        message.isController()?message.getControllerValue():0};
      midiFifo.finishedWrite(1);
    }
  }
  dsp.process(buffer,collectSettings());
  float sum=0;
  for(int ch=0;ch<buffer.getNumChannels();ch++){
    const float r=buffer.getRMSLevel(ch,0,buffer.getNumSamples());sum+=r*r;
  }
  level.store(std::sqrt(sum/juce::jmax(1,buffer.getNumChannels())),std::memory_order_relaxed);
}
bool SishhinDSPAudioProcessor::takeMidi(MidiEvent& e){
  int start1=0, size1=0, start2=0, size2=0;
  midiFifo.prepareToRead(1,start1,size1,start2,size2);
  if(size1==0)return false;
  e=midiQueue[static_cast<size_t>(start1)];midiFifo.finishedRead(1);return true;
}
float SishhinDSPAudioProcessor::getPitchHz() const {
  constexpr int window=2048;
  constexpr int stride=3;
  const auto write=tunerIndex.load(std::memory_order_acquire);
  if(write<window+100) return 0;
  std::array<float,4096> data{};
  const auto end=write;
  for(int i=0;i<4096;i++)data[static_cast<size_t>(i)]=tunerRing[static_cast<size_t>((end-4096u+static_cast<unsigned int>(i))%tunerRing.size())].load(std::memory_order_relaxed);
  float rms=0;
  for(int i=0;i<4096;i++)rms+=data[static_cast<size_t>(i)]*data[static_cast<size_t>(i)];
  if(rms/4096<.000025f)return 0;
  const int minLag=juce::jmax(1,static_cast<int>(getSampleRate()/1050));
  const int maxLag=juce::jmin(1400,static_cast<int>(getSampleRate()/55));
  float best=0;int bestLag=0;
  for(int lag=minLag;lag<=maxLag;lag++){
    float xy=0,aa=0,bb=0;
    for(int i=0;i<window;i+=stride){
      const float a=data[static_cast<size_t>(i)],b=data[static_cast<size_t>(i+lag)];
      xy+=a*b;aa+=a*a;bb+=b*b;
    }
    const float cor=xy/(std::sqrt(aa*bb)+1.e-10f);
    if(cor>best){best=cor;bestLag=lag;}
  }
  return best>.83f&&bestLag>0?static_cast<float>(getSampleRate()/bestLag):0;
}
void SishhinDSPAudioProcessor::setFromJson(const juce::var& json){
  if(!json.isObject())return;
  auto* obj=json.getDynamicObject();
  if(!obj)return;
  for(auto* param:getParameters()){
    const auto id=param->getName(256); // display name is equal to ID in makeLayout()
    if(!obj->hasProperty(juce::Identifier(id)))continue;
    auto v=obj->getProperty(juce::Identifier(id));
    float scalar=(id=="mode"?(v.toString()=="stereo"?1.f:0.f):
       v.isBool()?(static_cast<bool>(v)?1.f:0.f):static_cast<float>(static_cast<double>(v)));
    if(!std::isfinite(scalar))continue;
    auto* ranged=dynamic_cast<juce::RangedAudioParameter*>(param);
    if(ranged==nullptr)continue;
    ranged->setValueNotifyingHost(ranged->convertTo0to1(scalar));
  }
  if(auto eq=obj->getProperty("eq");eq.isArray()){
    for(int i=0;i<juce::jmin(9,eq.getArray()->size());i++){
      auto* p=apvts.getParameter("eq"+juce::String(i));
      if(p!=nullptr){
        const auto scalar=static_cast<float>(static_cast<double>((*eq.getArray())[i]));
        if(std::isfinite(scalar)) p->setValueNotifyingHost(p->convertTo0to1(scalar));
      }
    }
  }
}
juce::String SishhinDSPAudioProcessor::currentJson() const {
  const auto s=collectSettings();
  auto obj=std::make_unique<juce::DynamicObject>();
  #define PROP(x) obj->setProperty(#x,s.x)
  PROP(amp);PROP(input);PROP(output);PROP(gate);PROP(gateOn);PROP(transpose);
  obj->setProperty("mode",s.mode>.5f?"stereo":"mono");
  PROP(doubler);PROP(doublerOn);PROP(gain);PROP(bass);PROP(mid);PROP(treble);PROP(presence);PROP(master);
  PROP(bloomOn);PROP(bloom);PROP(edgeOn);PROP(drive);PROP(tone);PROP(level);PROP(weightOn);PROP(boost);
  PROP(cab);PROP(micA);PROP(micB);PROP(posA);PROP(posB);PROP(distA);PROP(distB);
  PROP(micLevelA);PROP(micLevelB);PROP(panA);PROP(panB);
  PROP(hp);PROP(lp);PROP(delayOn);PROP(delayMix);PROP(delayTime);PROP(feedback);
  PROP(reverbOn);PROP(reverbMix);PROP(decay);
  #undef PROP
  obj->setProperty("gateOn",s.gateOn>.5f);
  obj->setProperty("doublerOn",s.doublerOn>.5f);
  obj->setProperty("bloomOn",s.bloomOn>.5f);
  obj->setProperty("edgeOn",s.edgeOn>.5f);
  obj->setProperty("weightOn",s.weightOn>.5f);
  obj->setProperty("delayOn",s.delayOn>.5f);
  obj->setProperty("reverbOn",s.reverbOn>.5f);
  juce::Array<juce::var> eq;
  for(float v:s.eq)eq.add(v);
  obj->setProperty("eq",juce::var(eq));
  return juce::JSON::toString(juce::var(obj.release()),true);
}
void SishhinDSPAudioProcessor::loadIR(const juce::File& file){
  if(!file.existsAsFile())return;
  irFile=file;dsp.loadIr(file);
}
void SishhinDSPAudioProcessor::removeIR(){irFile=juce::File{};dsp.clearIr();}
void SishhinDSPAudioProcessor::getStateInformation(juce::MemoryBlock& out){
  auto state=apvts.copyState();state.setProperty("irFile",irFile.getFullPathName(),nullptr);
  auto xml=state.createXml();copyXmlToBinary(*xml,out);
}
void SishhinDSPAudioProcessor::setStateInformation(const void* bytes,int length){
  auto xml=getXmlFromBinary(bytes,length);
  if(xml!=nullptr){
    auto state=juce::ValueTree::fromXml(*xml);
    if(state.isValid()){
      auto path=state.getProperty("irFile").toString();
      apvts.replaceState(state);
      if(path.isNotEmpty())loadIR(juce::File(path));
      else removeIR();
    }
  }
}
juce::AudioProcessorEditor* SishhinDSPAudioProcessor::createEditor(){return new SishhinDSPAudioProcessorEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new SishhinDSPAudioProcessor();}
