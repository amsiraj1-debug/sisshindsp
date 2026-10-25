// Native bridge, loaded both on the website and inside the JUCE VST3/Standalone WebView.
// In a normal browser this file does not change the original audio implementation.
// Native mode is injected into the embedded index page by PluginEditor.cpp.
// The website itself is unaffected, and late-loading JUCE bridges are supported.
window.SISHHIN_NATIVE=!!(window.SISHHIN_FORCED_NATIVE || (window.__JUCE__&&window.__JUCE__.backend));
window.sishhinNativeEmit=(event,data={})=>{
  if(window.SISHHIN_NATIVE && window.__JUCE__?.backend?.emitEvent)
    window.__JUCE__.backend.emitEvent(event,data);
};
// Signal readiness only after the JUCE bridge exists. A one-shot retry fixes
// missed startup events in some WebView2 and WKWebView versions.
window.sishhinNativeAnnounceReady=()=>{
  if(!window.SISHHIN_NATIVE)return;
  let attempts=0;
  const announce=()=>{
    if(window.__JUCE__?.backend?.emitEvent){
      window.__JUCE__.backend.emitEvent('sishhinReady',{});
      return true;
    }
    return false;
  };
  if(announce())return;
  const retry=window.setInterval(()=>{
    if(announce() || ++attempts>=100)window.clearInterval(retry);
  },100);
};
class SishhinNativeAudio {
  constructor(){this.ctx={sampleRate:48000};this.sourceMode='native';this.muted=false;
    this.nativeLevel=0;this.nativePitch=0}
  update(settings){window.sishhinNativeEmit('sishhinState',JSON.stringify(settings))}
  level(){return this.nativeLevel}
  pitch(){return this.nativePitch||null}
  async start(){throw Error('Audio is supplied by your DAW or the standalone audio device')}
  stop(){}
}

// JUCE sends the exact original Japanese PNG artwork as data URLs so native
// WebViews can display it even when image subresource URLs are unavailable.
window.sishhinNativeLogoUrls = window.sishhinNativeLogoUrls || Object.create(null);
window.sishhinNativeInstallLogos = (inkUrl, creamUrl) => {
  if (!inkUrl || !creamUrl) return;
  window.sishhinNativeLogoUrls['logo-ink.png'] = inkUrl;
  window.sishhinNativeLogoUrls['logo-cream.png'] = creamUrl;
  const styleId = 'sishhin-native-embedded-logo-style';
  let style = document.getElementById(styleId);
  if (!style) { style = document.createElement('style'); style.id = styleId; document.head.appendChild(style); }
  // All three mini amps keep their actual image rather than a font fallback.
  style.textContent = '.native-mode .amp-card .mini-amp:after{background-image:url("' + creamUrl + '")!important}' +
    '.native-mode .amp-card:nth-child(1) .mini-amp:after{background-image:url("' + inkUrl + '")!important}';
  document.querySelectorAll('img.amp-logo').forEach(img => {
    const chosen = img.dataset.logo || (img.getAttribute('src') || '').split('/').pop();
    if (window.sishhinNativeLogoUrls[chosen]) img.src = window.sishhinNativeLogoUrls[chosen];
  });
};
// When the native page already includes embedded PNG data, install it before
// app.js renders the first amplifier. No external asset fetch is required.
if(window.SISHHIN_NATIVE && window.sishhinNativeLogoUrls['logo-ink.png'] &&
   window.sishhinNativeLogoUrls['logo-cream.png'])
  window.sishhinNativeInstallLogos(window.sishhinNativeLogoUrls['logo-ink.png'],
                                  window.sishhinNativeLogoUrls['logo-cream.png']);
