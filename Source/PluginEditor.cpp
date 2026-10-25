#include "PluginEditor.h"
#include <SishhinWebData.h>
#include <cstring>

namespace {
using Browser=juce::WebBrowserComponent;
Browser::Options optionsFor(SishhinDSPAudioProcessorEditor* self,
                            SishhinDSPAudioProcessor& processor,
                            std::function<void()> ready,
                            std::function<void()> updated,
                            std::function<void()> loadIr,
                            std::function<void()> clearIr) {
  juce::ignoreUnused(self);
  return Browser::Options{}
    .withBackend(Browser::Options::Backend::webview2)
    .withWinWebView2Options(Browser::Options::WinWebView2{}
       .withUserDataFolder(juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
           .getChildFile("SishhinDSP").getChildFile("WebView2")))
    .withNativeIntegrationEnabled()
    .withEventListener("sishhinReady",[ready](juce::var){ready();})
    .withEventListener("sishhinState",[&processor,updated](juce::var value){
       const auto obj=juce::JSON::parse(value.toString());
       if(obj.isObject()){processor.setFromJson(obj);updated();}
    })
    .withEventListener("sishhinIR",[loadIr](juce::var){loadIr();})
    .withEventListener("sishhinClearIR",[clearIr](juce::var){clearIr();})
    .withResourceProvider([](const juce::String& s){
       return SishhinDSPAudioProcessorEditor::getResource(s);
    });
}
}
std::optional<juce::WebBrowserComponent::Resource>
SishhinDSPAudioProcessorEditor::getResource(const juce::String& path) {
  auto p=path.upToFirstOccurrenceOf("?",false,false).trimCharactersAtStart("/");
  if(p.isEmpty())p="index.html";
  juce::String name,mime;
  if(p=="index.html") {name="index_html";mime="text/html";}
  else if(p=="app.js") {name="app_js";mime="text/javascript";}
  else if(p=="audio.js") {name="audio_js";mime="text/javascript";}
  else if(p=="native-bridge.js") {name="native_bridge_js";mime="text/javascript";}
  else if(p=="pitch-worklet.js") {name="pitch_worklet_js";mime="text/javascript";}
  else if(p=="style.css") {name="style_css";mime="text/css";}
  else if(p=="assets/logo-cream.png") {name="logo_cream_png";mime="image/png";}
  else if(p=="assets/logo-ink.png") {name="logo_ink_png";mime="image/png";}
  else return std::nullopt;
  int bytes=0;
  auto* data=SishhinWebData::getNamedResource(name.toRawUTF8(),bytes);
  if(data==nullptr||bytes<=0)return std::nullopt;
  Browser::Resource resource;
  resource.mimeType=mime;
  resource.data.resize(static_cast<size_t>(bytes));
  std::memcpy(resource.data.data(),data,static_cast<size_t>(bytes));
  return resource;
}

SishhinDSPAudioProcessorEditor::SishhinDSPAudioProcessorEditor(SishhinDSPAudioProcessor& p)
  : AudioProcessorEditor(&p),processor(p),
    browser(optionsFor(this,p,[this]{pageReady=true;sendEmbeddedLogos();sendState();},
                            [this]{lastSentState=processor.currentJson();},
                            [this]{chooseIr();},
                            [this]{processor.removeIR();})) {
  setResizable(true,true);
  setResizeLimits(900,600,2200,1500);
  setSize(1042,633);
  addAndMakeVisible(browser);
  browser.goToURL(Browser::getResourceProviderRoot());
  startTimerHz(8);
}
SishhinDSPAudioProcessorEditor::~SishhinDSPAudioProcessorEditor(){stopTimer();chooser.reset();}
void SishhinDSPAudioProcessorEditor::resized(){browser.setBounds(getLocalBounds());}
void SishhinDSPAudioProcessorEditor::sendJavascript(const juce::String& js){
  if(pageReady)browser.evaluateJavascript(js);
}
// The PNG bytes are compiled into the plugin, avoiding WebView subresource
// routing differences across Edge WebView2 and WKWebView.
void SishhinDSPAudioProcessorEditor::sendEmbeddedLogos(){
  int inkBytes=0, creamBytes=0;
  const auto* ink=SishhinWebData::getNamedResource("logo_ink_png",inkBytes);
  const auto* cream=SishhinWebData::getNamedResource("logo_cream_png",creamBytes);
  if(ink==nullptr||cream==nullptr||inkBytes<=0||creamBytes<=0)return;
  const juce::String inkUrl="data:image/png;base64,"+juce::Base64::toBase64(ink,static_cast<size_t>(inkBytes));
  const juce::String creamUrl="data:image/png;base64,"+juce::Base64::toBase64(cream,static_cast<size_t>(creamBytes));
  sendJavascript("window.sishhinNativeInstallLogos("+juce::JSON::toString(juce::var(inkUrl))+","+
                 juce::JSON::toString(juce::var(creamUrl))+");");
}
void SishhinDSPAudioProcessorEditor::sendState(){
  lastSentState=processor.currentJson();
  sendJavascript("window.sishhinNativeApplyState("+lastSentState+");");
}
void SishhinDSPAudioProcessorEditor::timerCallback(){
  if(!pageReady)return;
  const auto now=processor.currentJson();
  if(lastSentState!=now)sendState();
  SishhinDSPAudioProcessor::MidiEvent ev;
  int count=0;
  while(count++<32 && processor.takeMidi(ev)) {
    sendJavascript("window.sishhinNativeMidi("+juce::String(ev.status)+","+
      juce::String(ev.number)+","+juce::String(ev.value)+");");
  }
  sendJavascript("window.sishhinNativePulse("+
    juce::String(processor.getLevel(),6)+","+
    juce::String(processor.getCurrentSampleRate(),0)+","+
    juce::String(processor.getPitchHz(),2)+","+
    juce::String(processor.getIrName().quoted())+");");
}
void SishhinDSPAudioProcessorEditor::chooseIr(){
  chooser=std::make_unique<juce::FileChooser>("Load cabinet impulse response (WAV or AIFF)",
                                             juce::File{},"*.wav;*.aiff;*.aif");
  juce::Component::SafePointer<SishhinDSPAudioProcessorEditor> weak(this);
  chooser->launchAsync(juce::FileBrowserComponent::openMode |
                       juce::FileBrowserComponent::canSelectFiles,[weak](const juce::FileChooser& f){
    if(weak==nullptr)return;
    const auto ir=f.getResult();
    if(ir.existsAsFile() && ir.getSize()<=50*1024*1024)weak->processor.loadIR(ir);
  });
}
