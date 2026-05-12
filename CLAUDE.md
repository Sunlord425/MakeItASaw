# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

A DAW plugin that converts any arbitrary pitched monophonic input signal into a sawtooth wave. The core constraint is **no FFT or pitch-tracking algorithms** — the conversion must be zero-latency (or near-zero), achieved entirely through analog-style DSP signal processing.

Plugin format targets: VST3, AU (required), Standalone. Built with **JUCE 8.0.6** via CMake FetchContent.

## Signal Chain Architecture

The conversion pipeline, in order:

1. **Comparator** — outputs high when input > 0, low when input < 0, producing a pulse wave
2. **DC bias correction** — fixed subtraction to center the pulse wave around zero (removes asymmetry)
3. **Asymmetric slew limiter** — slew only on the rising edge; produces a ramp-up followed by a sharp drop, forming the sawtooth shape
4. **Amplitude normalization** — compensates for higher-pitched notes being quieter (less rise time between rarefactions)
5. **Envelope follower** — scales the normalized output to match the amplitude envelope of the input signal

### Known Limitation & Proposed Fix

During the negative half-cycle of the input, the comparator output stays at zero, producing a "half-saw / half-flat" waveform. A proposed fix:

- Take a 90°-phase-shifted copy of the input signal
- Optionally invert it (multiply by −1)
- Sum it with the primary signal path so the rise of one fills the gap of the other
- Verify that summing does not alter perceived pitch

### Nice-to-Have Feature

Allow the user to mix in detuned copies of the output sawtooth with the dry signal (unison/detune effect).

## Build Commands

```bash
# Configure (downloads JUCE ~first run only)
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Build all formats (AU, VST3, Standalone)
cmake --build build --config Debug -- -j$(sysctl -n hw.logicalcpu)
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
Source/
├── DSP/SawConverter.h     — header-only DSP class, all signal chain logic here
├── PluginProcessor.h/cpp  — JUCE AudioProcessor; owns a SawConverter per channel
└── PluginEditor.h/cpp     — minimal UI (dark background + "SAW" label)
```

**DSP design notes** (`SawConverter.h`):

The core algorithm is a **rising zero-crossing detector** (not the original comparator/DC-bias/slew chain). At each rising zero crossing of the pre-processed input, the ramp resets to −1 and then rises continuously through the full period, producing a complete sawtooth per cycle rather than a half-saw / half-flat shape.

- `armed` flag provides hysteresis: a reset only fires if the signal has gone negative since the last reset, preventing false re-triggers on the same rising edge or from harmonics.
- The drive stage upstream (tanh saturation in `processBlock`) is critical: it squares up the signal so each fundamental period has exactly one dominant rising zero crossing.
- Slew rate is calibrated so the ramp covers its full −1→+1 range in one period at 25 Hz. Notes above 25 Hz rise less far; the peak-follower normalizer compensates.
- The peak follower operates on `slewPrev + 1.0` (positive-shifted to [0, 2]) so it tracks only the upward excursion. Noise gate threshold is 0.005 (≈ −46 dB).
- A 1st-order DC blocker (R = 0.9995, ≈3.5 Hz cutoff) follows normalization as a safety measure; a full sawtooth has near-zero DC so it rarely does significant work.
- The input envelope follower's 3 ms attack suppresses any normalization transients at note onset.
- `juce::ScopedNoDenormals` is set in `processBlock` to prevent denormal CPU spikes.
