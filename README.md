# SishhinDSP – Windows / macOS VST3 and Standalone

## Upload source to your GitHub repository

The project in this ZIP is already organized for upload to the **root** of `amsiraj1-debug/sisshindsp2`. **Do not commit a single ZIP file or nest this project in another folder**: `.github/workflows/build.yml` must be at the repository root.

1. Extract the full ZIP on a Windows computer.
2. Open its extracted folder (where `CMakeLists.txt` exists).
3. Right-click `UPLOAD_TO_GITHUB.ps1` and use PowerShell to run it; if scripts are blocked, open PowerShell in the extracted folder and run `powershell -ExecutionPolicy Bypass -File .\UPLOAD_TO_GITHUB.ps1`. Requires Git for Windows and GitHub sign-in.
4. Check **https://github.com/amsiraj1-debug/sisshindsp2/actions**. The push should trigger the build. Alternatively choose **Run workflow**.
5. After both jobs succeed, download the `.zip` build artifacts from the workflow run page.

See [BUILD_TROUBLESHOOTING.md](BUILD_TROUBLESHOOTING.md) for diagnosing `exit code 1` from actual job logs.

# SishhinDSP — White Edition | VST3 + Standalone

This source ZIP converts the uploaded SishhinDSP guitar website into a JUCE 8 audio plug-in **for Windows and macOS**, and a **standalone desktop audio application** on both platforms. The original HTML/CSS/images remain embedded in the executable, not redrawn as a new interface. The only native-only UI addition is the custom cabinet IR loader. Audio processing is **native C++**, not Web Audio inside the DAW.

## What is included

- 3 amp voicings (Porcelain / Katana / Monolith), pre-effects (Bloom, Edge, Weight), delay (Echo), reverb (Halo), noise gate, nine-band EQ, filters, two mic-level/pan controls, input/output, basic transpose and doubler.
- 72 factory presets are in the existing `dist/app.js`, plus user-preset import/export and save support using the embedded WebView's storage.
- Native stereo/mono handling, host-automatable controls, input tuner and native signal metering, MIDI CC learn and Program Change forwarded from the DAW to the open plug-in interface.
- Cabinet WAV / AIFF IR loader in the **Cabinet** page of the native interface. IR files are loaded asynchronously using JUCE Convolution and restored by absolute file path when the DAW project reloads on the same computer.
- Original website playable as before: `dist/index.html` uses its Web Audio engine outside JUCE.

## Build via GitHub Actions — no local compiler needed

1. Create a new **GitHub repository** and upload **the extracted contents** of this ZIP into its root. Do not upload just the ZIP file by itself.
2. Open **Actions** → **Build SishhinDSP (Windows + macOS)** → **Run workflow**.
3. After the green build completes, open the workflow run → **Artifacts** and download `SishhinDSP-windows-x64` and/or `SishhinDSP-macos-universal`.
4. Each build artifact contains a `VST3` directory and a `Standalone` directory. The Windows VST3 is a `.vst3` folder; copy the whole folder.

Windows: place `SishhinDSP.vst3` at `C:\Program Files\Common Files\VST3\` and refresh the DAW plug-in list. Run `SishhinDSP.exe` for the standalone. The native UI uses Microsoft Edge WebView2 runtime, normally installed on current Windows versions.

macOS: place `SishhinDSP.vst3` at `~/Library/Audio/Plug-Ins/VST3/` (or `/Library/Audio/Plug-Ins/VST3/`) and refresh the DAW. Run `SishhinDSP.app` for standalone. The GitHub builds are **unsigned and not notarized**, so macOS may block them until allowed under Privacy & Security. For commercial distribution, sign and notarize them using your own Apple Developer credentials.

## Building locally

- JUCE 8 is automatically downloaded by CMake using FetchContent. The project pins `8.0.16-security`.
- Windows: Visual Studio 2022 C++ desktop workload, CMake 3.24+, WebView2 SDK NuGet package (the workflow downloads the SDK automatically).
- macOS: Xcode, CMake 3.24+.

Build commands:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 3
```

Note: on Windows, CMake may need `-DJUCE_WEBVIEW2_PACKAGE_LOCATION=/path/to/nuget/packages`, or simply use the provided workflow.

## Controls / functionality notes

- In a DAW, audio flows from your **DAW insert / track**, not from the website's "Connect guitar" button. In standalone, configure the audio/MIDI device in the app's audio settings. The website version still supports live microphone/demo audio as before.
- MIDI hardware is routed by the DAW/standalone audio device manager. The embedded editor processes CC and Program Change while **the editor is open**; this MIDI mapping is not a fully editor-independent, persistent DAW MIDI-mapping subsystem.
- The native amp cabinet/filter/distortion engine is an **original approximation**, not neural capture or licensed brand modeling. It does not reproduce the exact Web Audio processing sample-for-sample.
- Transpose is a simple short-window pitch shifter and may produce noticeable artifacts on chords or large intervals. The doubler is short-delay based, and the mic-placement controls are approximate voicings rather than measured IR positioning. This is an initial native implementation, not a finished commercial DSP package.
- Downloaded cabinet IRs are loaded on demand; the IR audio bytes are **not embedded into DAW session state**. Keep the WAV/AIFF in its original file path.
- To export user presets, use the Preset Library **Export my presets** control. Embedded browser storage is local to that computer.

## Technical layout

`dist/` — unchanged core website layout and graphics, plus small native bridge additions.

`Source/` — JUCE AudioProcessor, WebView editor and C++ DSP (amps, EQ, delay, reverb, pitch-shift, convolution).

`CMakeLists.txt` — VST3 + Standalone configuration and embedded HTML resources.

`.github/workflows/build.yml` — official GitHub Actions Windows x64 and macOS universal build and artifact packaging.

## Licensing

JUCE is offered under commercial/open-source license terms (including AGPLv3 options); check the current [JUCE licensing terms](https://juce.com/legal/) before distribution. This ZIP does not vendor JUCE binaries. The uploaded artwork remains your original project assets. External Google Fonts, if available, may be loaded by the website; system fonts are the fallback.

## Build confidence

All website JS syntax and source/bridge checks can be run locally (`node scripts/validate-web.js`). **Native Windows/macOS compilation has not been run in the current environment** because the JUCE dependency and target OS toolchains are not available here. A GitHub Actions run is required to confirm that the compiler, linker, WebView and host plug-in tests pass. Review the workflow log and do not assume the binary has been tested just because the source ZIP is complete.


## Native UI / Japanese logo restoration (October 2026)

The VST3 and standalone editor opens directly into the compact studio panel
from the supplied screenshot (approximately 1042 × 633), hiding only the
website-only hero and header *inside native builds*. The original white theme,
three amplifier appearances, controls and Japanese **失神** artwork are kept.
The original two logo PNGs are bundled into native binaries, and the editor
passes them as embedded image data URLs to the WebView for reliable rendering
on Windows and macOS. The standard browser version retains its original page.

The earlier native JUCE API compiler fixes are included. This UI update is
source code and has **not yet been rebuilt or visually verified in the native
VST3/standalone hosts**. Push the extracted files to GitHub to trigger the
Windows/macOS CI builds.
