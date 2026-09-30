# The Amazing Notta

A JUCE 8 VST3/AU visual placebo mastering plugin. The audio processor is a true
in-place pass-through: it does not write to the audio buffer. All meters, drift,
bypass styling, presets, menus, warnings, and mini-game behavior are visual only.

## Requirements

- CMake 3.22 or newer and Git
- Windows: Visual Studio 2022 with the Desktop development with C++ workload
- macOS: Xcode and the Command Line Tools
- Linux: Clang or GCC, Make/Ninja, and the JUCE Linux GUI/audio development packages
- Internet access for the first configure; CMake fetches JUCE 8.0.6 from GitHub

## Configure and build

Run these commands from the repository root.

### Windows (PowerShell, Visual Studio 2022)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
```

### macOS (Xcode; builds VST3, AU, and Standalone)

```sh
cmake -S . -B build -G Xcode -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

### Linux (Ninja; builds VST3 and Standalone; AU is macOS-only)

On Ubuntu/Debian, install the JUCE GUI and audio development dependencies first:

```sh
sudo apt update
sudo apt install build-essential clang cmake ninja-build pkg-config git \
	libasound2-dev libfontconfig1-dev libfreetype6-dev libgl1-mesa-dev libgtk-3-dev \
	libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev \
	libxrandr-dev libxrender-dev
```

Then configure and build:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

JUCE's `COPY_PLUGIN_AFTER_BUILD` option copies built plug-ins to the platform's
default user plug-in location when supported. Build artifacts also remain under
`build/TheAmazingNotta_artefacts/Release`.

The editor includes intentionally theatrical controls. "Power" and all other
controls affect only the GUI; none changes audio. The audio path accepts matching
mono or stereo input/output layouts and reports zero plug-in latency.

## Validation

On macOS, after building and installing the AU component, run Apple's validator:

```sh
auval -v aufx TAn1 Ntta
```

For VST3, install Steinberg's `pluginval` and run it against the built bundle, for
example:

```sh
pluginval --strictness-level 5 "build/TheAmazingNotta_artefacts/Release/VST3/The Amazing Notta.vst3"
```