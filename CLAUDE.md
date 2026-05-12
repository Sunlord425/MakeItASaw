# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

A DAW plugin that converts any arbitrary pitched monophonic input signal into a sawtooth wave. The core constraint is **no FFT or pitch-tracking algorithms** — the conversion is achieved entirely through analog-style DSP signal processing.

Plugin format targets: VST3, AU (macOS only), Standalone. Built with **JUCE 8.0.6** via CMake FetchContent. Releases are built by GitHub Actions and distributed as signed installers (`.pkg` for macOS, NSIS `.exe` for Windows).

## Signal Chain Architecture

```
Input → IN GAIN → TONE (1-pole LP) → DRIVE (tanh) → ZCD converter
      → UNISON mix → ENV FILTER (biquad LP) → OUT GAIN → Output
```

**ZCD converter** (`SawConverter.h`): rising zero-crossing detector. At each rising zero crossing, the ramp resets to −1 and rises continuously through the full period, producing a complete sawtooth per cycle.

- `armed` flag provides hysteresis: a reset fires only if the signal has gone below −0.01 since the last reset. This ignores tiny harmonic excursions that would cause false triggers.
- When `smoothedPeriod` is valid, a minimum gap of `smoothedPeriod * 0.5` is enforced between resets, preventing harmonics within the same cycle from triggering spurious resets on high notes.
- Slew rate calibrated for 25 Hz (ramp covers −1→+1 in one full period). Higher notes rise less far; the peak-follower normalizer compensates.
- Peak follower operates on `slewPrev + 1.0` (shifted to [0, 2]); noise gate threshold 0.005 (≈ −46 dB).
- 1st-order DC blocker (R = 0.9995, ≈ 3.5 Hz) follows normalization as a safety net.
- Input envelope follower (3 ms attack, 80 ms release) scales the output to follow the input dynamics.

**TONE pre-filter** (`processBlock`): 1-pole LP applied before DRIVE to suppress harmonics before they reach the ZCD. Cutoff 100 Hz – 20 kHz; default fully open. Rolling it down reduces false ZCD triggers on guitar, particularly on high notes.

**DRIVE**: tanh saturation applied after TONE. Squares up the signal so each fundamental period has one dominant rising zero crossing.

**UNISON** (`PitchShifter.h`): up to 8 delay-based pitch shifters per channel. Ratios spread evenly from −detune to +detune cents. Mix uses equal-power normalization (`sqrt(1 + wet·N)`) to keep perceived loudness stable as voice count grows. Crossfades use cosine windowing over 128 samples to reduce glitches.

**ENV FILTER**: bilinear-transform biquad LP applied after unison mix. Cutoff is modulated by a per-channel envelope follower (3 ms attack, 150 ms release) tracking the saw output. SVF coefficients are computed once per block (one `sin`/`cos` call per channel). FREQ sets the peak cutoff at maximum envelope; SENS controls the sweep depth in octaves (0 = bypass); RES controls Q (0.5→10), which raises a resonant peak for the wah character. The biquad is unconditionally stable up to Nyquist (the old Chamberlin SVF was used previously and became unstable above ~sr/6).

## Releasing

Tag a commit to trigger the CI build and create a GitHub Release with installers attached:

```bash
git tag v0.1.0
git push origin v0.1.0
```

The workflow (`.github/workflows/build.yml`) runs two jobs — `build-macos` (macos-14) and `build-windows` (windows-latest) — then a `release` job that attaches the artifacts. macOS job produces `Saw-macOS.pkg` (combined AU + VST3 via `productbuild`); Windows job produces `Saw-0.1.0-Windows.exe` via NSIS. Packaging scripts live in `packaging/`.

## Build Commands

```bash
# Configure (downloads JUCE ~first run only)
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Build all formats (AU, VST3, Standalone)
cmake --build build --config Debug -- -j$(sysctl -n hw.logicalcpu)

# Release build (for distribution)
cmake -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --config Release -- -j$(sysctl -n hw.logicalcpu)
```

Artifacts land in `build/SawPlugin_artefacts/Debug/{AU,VST3,Standalone}/`.

**Install & validate the AU:**
```bash
# Use ditto (not cp) to preserve extended attributes, then re-sign.
# cp -r silently strips the code signature and causes auval SIGKILL on macOS 15.
rm -rf ~/Library/Audio/Plug-Ins/Components/Saw.component
ditto build/SawPlugin_artefacts/Debug/AU/Saw.component ~/Library/Audio/Plug-Ins/Components/Saw.component
codesign -s - --force ~/Library/Audio/Plug-Ins/Components/Saw.component
killall -9 AudioComponentRegistrar   # force registry rescan
auval -v aufx SawP Lmaz              # type=aufx subtype=SawP manufacturer=Lmaz
```

## Code Structure

```
packaging/
├── mac/
│   ├── postinstall      — kills AudioComponentRegistrar after pkg install
│   └── distribution.xml — productbuild descriptor (AU + VST3 combined pkg)
└── windows/
    └── installer.nsi    — NSIS script; installs to %CommonProgramFiles64%\VST3

Source/
├── DSP/
│   ├── SawConverter.h   — ZCD-based mono→sawtooth converter (header-only)
│   └── PitchShifter.h   — delay-based pitch shifter for unison voices (header-only)
├── PluginProcessor.h/cpp — AudioProcessor: owns converters, shifters, biquad state
└── PluginEditor.h/cpp    — Three-section UI: GAIN STAGE | UNISON | ENV FILTER
```

**Parameters** (APVTS IDs):

| ID | Section | Range | Default |
|----|---------|-------|---------|
| `inputGain` | Gain Stage | −12 to +24 dB | 0 dB |
| `tone` | Gain Stage | 100 Hz – 20 kHz | 20 kHz (open) |
| `drive` | Gain Stage | 0 – 100% | 0% |
| `outputGain` | Gain Stage | −20 to +12 dB | 0 dB |
| `voices` | Unison | 1 – 8 (int) | 3 |
| `detune` | Unison | 0 – 50 cents | 0 cts |
| `unisonMix` | Unison | 0 – 100% | 0% |
| `envFreq` | Env Filter | 200 Hz – 8 kHz | 4 kHz |
| `envSens` | Env Filter | 0 – 100% | 0% (bypass) |
| `envRes` | Env Filter | 0 – 100% | 0% |

## Known Issues / Outstanding Work

- **Attack latency feel**: the ZCD needs at least one zero crossing before locking on, so the first cycle or two of a new note outputs a partial ramp rather than a clean saw. Makes the onset feel slightly slow.
- **Residual crackling on high notes**: the half-period guard and TONE filter reduce false triggers significantly, but harmonics can still slip through with high-gain sources or TONE fully open.
