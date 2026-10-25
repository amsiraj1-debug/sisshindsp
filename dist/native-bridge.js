// Native bridge, loaded both on the website and inside the JUCE VST3/Standalone WebView.
// In a normal browser this file does not change the original audio implementation.
window.SISHHIN_NATIVE=!!(window.__JUCE__&&window.__JUCE__.backend);
window.sishhinNativeEmit=(event,data={})=>{
  if(window.SISHHIN_NATIVE)window.__JUCE__.backend.emitEvent(event,data);
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
window.sishhinNativeLogoUrls = Object.create(null);
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
