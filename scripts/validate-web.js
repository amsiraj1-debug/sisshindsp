// Lightweight source contract checks; actual native builds are performed in GitHub Actions.
const fs=require('fs');
const assert=require('node:assert/strict');
const src=fs.readFileSync('dist/index.html','utf8');
const app=fs.readFileSync('dist/app.js','utf8');
const bridge=fs.readFileSync('dist/native-bridge.js','utf8');
for(const file of ['style.css','audio.js','native-bridge.js','app.js'])
 assert.ok(src.includes(`src="${file}"`)||src.includes(`href="${file}"`),file+' must be linked');
for(const id of ['view-content','midi-open','presets-dialog','tuner-dialog','status-led'])
 assert.ok(src.includes(`id="${id}"`),'missing original element '+id);
for(const symbol of ['sishhinNativeApplyState','sishhinNativePulse','sishhinNativeMidi','sishhinReady','sishhinState'])
 assert.ok(app.includes(symbol)||bridge.includes(symbol),'missing bridge '+symbol);
assert.ok(app.includes('const names=['),'presets are present');
assert.ok(app.includes('data-ir=\\"load\\"')||app.includes('data-ir="load"'),'native IR button present');
console.log('Web asset/bridge contract checks passed.');
