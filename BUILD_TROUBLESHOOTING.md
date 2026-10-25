# GitHub Actions build troubleshooting

## No runs appear
If the GitHub repository says *This repository is empty*, nothing has been pushed yet. Extract the package and run `UPLOAD_TO_GITHUB.ps1` on Windows (with Git installed). The `.github/workflows/build.yml` file and `CMakeLists.txt` must be at the repository root, not inside an extra `sishhindsp-crossplatform` folder.

## Exit code 1
This status alone does not tell which command failed. In GitHub select **Actions → Build SishhinDSP (Windows + macOS) → the failed run → failed job**. Expand the first step with a red X and copy the first detailed `error:` / `CMake Error` message and 15 lines around it. Different messages require different fixes.

## Jobs in this workflow
- `Verify required source files` detects incorrect nested upload paths.
- `Validate embedded web interface` checks HTML, scripts, and native bridge.
- `Install WebView2 SDK` (Windows only) downloads Microsoft headers/libs.
- `Configure Windows` and `Configure macOS Universal` fetch JUCE and generate build files.
- `Build VST3 and Standalone` compiles native binaries.
- `Package outputs` verifies generated VST3 and standalone bundles.
- `Upload compiled binaries` publishes downloadable Action artifacts.

JUCE 8.0.16-security is automatically fetched by CMake when GitHub runners have Internet. Native builds have not been validated in this environment. macOS packages are unsigned and may require manual trust when testing on a Mac.
