# MixSense — AI Mixing Assistant VST3 & Standalone Plugin

**MixSense** is an intelligent Mixing Assistant VST3 and Standalone audio plugin designed for **FL Studio**, Ableton Live, Reaper, Cubase, and other DAWs. Built with **C++17, JUCE 8, and CMake**.

MixSense acts as a virtual mix engineer sitting on your Master Bus or individual mixer tracks. It monitors your track in real time, diagnoses tonal balance issues, low-end mud, harsh resonances, phase problems, and squashed dynamics, explains how to fix them in FL Studio, and provides **1-Click Auto-Fix DSP** with an adjustable **Strength (0–100%)** slider.

---

## Key Features

### 1. 6-Pillar Mixing Diagnostic Engine
- **Tonal Balance & 6-Band Frequency Matching**: Sub (20–60 Hz), Bass (60–250 Hz), Low-Mid / Mud (250–500 Hz), Mid (500–2 kHz), Upper-Mid (2–6 kHz), and Air (6–20 kHz) compared against calibrated genre targets.
- **Boomy Sub & Bass Buildup Warning**: Flags low-end excess eating up headroom and triggering premature limiter pumping.
- **Boxy / Muddy Low-Mid Detection**: Identifies 250–500 Hz buildup and suggests exact EQ dips and high-passing non-bass instruments.
- **Harshness & Ringing Resonance Hunter**: 4096-point FFT detects narrow harsh frequency spikes and identifies the exact frequency (in Hz) and musical note (e.g. `3520 Hz (A7)`).
- **Stereo Phase Cancellation Guard**: Monitors Pearson phase correlation ($r \in [-1, +1]$) and warns when stereo widening is causing mono cancellation.
- **Sub-120Hz Stereo Bass Detector**: Detects out-of-phase side energy below 120 Hz and provides a 1-click **Mono Bass** crossover filter.
- **Dynamic Range & Crest Factor (Peak - RMS)**: Alerts when a mix is over-compressed (< 6 dB) or transient-spiky (> 15 dB).
- **Headroom & Clipping Monitor**: True Peak and LUFS tracking with pre-master (-6 dBFS) vs. streaming master target verification.

### 2. 1-Click Corrective DSP Processor
- **Subsonic HPF**: 24 dB/oct Butterworth filter (20–40 Hz).
- **Bass Mono Maker**: Mid/Side processor with 120 Hz high-pass on the Side channel.
- **5-Band Corrective Parametric EQ**: Dynamically configured from the diagnosed issues.
- **Glue Bus Compressor**: Smooth transient taming.
- **Assistant Strength Slider (0–100%)**: Scale all suggested EQ cuts/boosts and compression smoothly.
- **Delta Mode ("Listen to Fixes")**: Audition `Dry - Processed` to hear *only* the mud, rumble, and harshness being removed.
- **A/B Bypass**: Instant click-free comparison.

### 3. Target Profiles & Stage Modes
- **Genre Profiles**:
  - `Modern Pop` (balanced commercial curve)
  - `EDM / Club` (extended sub punch + crisp air + strict mono bass)
  - `Hip-Hop / Trap` (heavy 808 sub emphasis + scooped low-mids for vocal clarity)
  - `Rock / Acoustic` (natural warmth and midrange focus)
  - `Vocal / Lead Bus` (high-passed sub + speech presence boost)
- **Stage Modes**:
  - `Pre-Master (-6 dB)`: Preserves dynamic headroom for mastering engineers.
  - `Mastering (LUFS)`: Calibrated for commercial streaming loudness (-14 to -9 LUFS).

---

## Using in FL Studio

1. Open **FL Studio** and go to **Options -> Manage Plugins**.
2. Ensure `C:\Program Files\Common Files\VST3` is in your plugin search paths, then click **Find installed plugins**.
3. In the FL Studio Mixer, add **MixSense** on your **Master Track** (Slot 1 or Slot 10) or on any instrument/vocal submix.
4. Select your **Genre Profile** (e.g., *Hip-Hop / 808* or *Modern Pop*).
5. Press **Play** in FL Studio. MixSense immediately displays real-time tonal balance, crest factor, phase correlation, and issue cards.
6. Click **SMART LISTEN** for a 7-second deep-listen capture across your drop/chorus, or click **1-CLICK FIX** on any issue card (or **APPLY ALL FIXES**).
7. Adjust the **Strength** slider or toggle **Delta** to hear the exact mud and harshness being carved away.

---

## Building from Source

### Prerequisites
- CMake 3.22+
- C++17 compatible compiler (`g++`, `clang++`, or `MSVC`)
- ALSA and X11 development libraries (on Linux: `libasound2-dev libfreetype6-dev libgl1-mesa-dev libx11-dev libxinerama-dev libxext-dev`)

### 1. Build and Run Automated Tests
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target MixSenseTests
./build/MixSenseTests
```

### 2. Build Linux VST3 & Standalone Application
```bash
cmake --build build --target MixSense_Standalone MixSense_VST3
# Run Standalone App:
./build/MixSense_artefacts/Release/Standalone/MixSense
```

### 3. Build Windows VST3 on Linux (MinGW-w64)
```bash
cmake -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-win --target MixSense_VST3
```
The resulting `MixSense.vst3` bundle is located in `build-win/MixSense_artefacts/Release/VST3/MixSense.vst3`.

