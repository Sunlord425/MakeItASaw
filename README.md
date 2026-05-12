# MakeItASaw

An audio effect plugin that converts any monophonic pitched signal — guitar, bass, voice, synth — into a sawtooth wave in real time, without pitch tracking or FFT. The conversion is entirely analog-style DSP: a rising zero-crossing detector resets a ramp at each fundamental cycle.

| Platform | Formats |
|----------|---------|
| macOS | AU, VST3, Standalone |
| Windows | VST3, Standalone |

## Download

Pre-built installers are attached to each [GitHub Release](https://github.com/Sunlord425/MakeItASaw/releases):

- **macOS** — `MakeItASaw-macOS.pkg`: double-click to install. Installs AU to `/Library/Audio/Plug-Ins/Components/` and VST3 to `/Library/Audio/Plug-Ins/VST3/`. You can deselect either format during installation.
- **Windows** — `MakeItASaw-x.x.x-Windows.exe`: run as Administrator. Installs VST3 to `C:\Program Files\Common Files\VST3\`.

After installing on macOS, restart your DAW (or run `killall -9 AudioComponentRegistrar` in Terminal) to force a plugin rescan.

---

## Controls

The UI is divided into three sections.

### Gain Stage

| Knob | What it does |
|------|-------------|
| **IN GAIN** | Input level before any processing (−12 to +24 dB). Boost if the source is quiet; the ZCD needs a clear signal to lock on. |
| **TONE** | 1-pole low-pass pre-filter (100 Hz – 20 kHz). Rolling it down removes upper harmonics before the zero-crossing detector sees them, reducing false triggers and crackling — especially useful for guitar or instruments with strong overtones. Default is fully open. |
| **DRIVE** | Soft-clip saturation (tanh) applied after TONE. Squares up the waveform so each period has one clean, dominant zero crossing. A moderate amount of drive generally improves tracking stability and sustain, particularly on guitar. |
| **OUT GAIN** | Output level after all processing (−20 to +12 dB). |
| **WET** | Dry/wet blend (0 – 100%). At 0% the plugin is fully bypassed and you hear the original signal; at 100% you hear only the processed sawtooth output. Default is 100%. |

### Unison

Adds detuned pitch-shifted copies of the sawtooth output, creating a chorus/unison effect.

| Knob | What it does |
|------|-------------|
| **VOICES** | Total voice count including the dry saw (1 – 8). At 1, the unison section is bypassed entirely. Voices are spread evenly from −DETUNE to +DETUNE cents. |
| **DETUNE** | Spread of the detuned voices in cents (0 – 50). Higher values give a wider, more dramatic chorus; lower values give subtle thickening. |
| **MIX** | Blend of detuned voices into the signal (0 – 100%). Uses equal-power mixing so perceived loudness stays consistent regardless of voice count. |

### Env Filter

An envelope-controlled resonant low-pass filter (auto-wah). The filter's cutoff tracks the amplitude of the saw output — louder playing opens the filter, quieter playing closes it.

| Knob | What it does |
|------|-------------|
| **FREQ** | Peak cutoff frequency reached at maximum envelope level (200 Hz – 8 kHz). Sets the "bright" position of the wah sweep. |
| **SENS** | Sensitivity / sweep depth. At 0% the filter is bypassed. Higher values increase the octave range swept between quiet and loud playing (up to 3 octaves at 100%). |
| **RES** | Resonance (Q). At 0% the filter is a gentle roll-off. Higher values add a resonant peak at the cutoff that gives the classic wah character. |

---

## Tips for Best Results

**Signal preparation matters most.** The converter works by detecting rising zero crossings of the fundamental. Anything that obscures those crossings — strong harmonics, noise, chords — causes glitches.

- **Use DRIVE.** Even a moderate amount (20–40%) squares up the waveform and dramatically improves tracking, especially on guitar. This is the most impactful control for stability.
- **Roll TONE down if you hear crackling on high notes.** High harmonics cause false zero-crossing triggers. Start around 2–4 kHz and adjust by ear.
- **Play monophonically.** Two simultaneous pitches produce a combined waveform with unpredictable zero crossings.
- **Boosting IN GAIN helps quiet or high-impedance sources** lock on more reliably.
- **The envelope filter is most expressive at moderate SENS (40–70%) and FREQ around 1–3 kHz.** High RES gives a pronounced wah peak; keep it under 60–70% unless you specifically want the resonance to sing.
- **For unison width without muddiness**, keep DETUNE under 20 cents and VOICES at 3–5.

---

## Building from Source

**macOS**: CMake ≥ 3.22, Xcode, internet access on first build (fetches JUCE 8.0.6).  
**Windows**: CMake ≥ 3.22, Visual Studio 2022 (or Build Tools), internet access on first build.

```bash
git clone https://github.com/Sunlord425/MakeItASaw.git
cd MakeItASaw

# Debug build
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug -- -j$(sysctl -n hw.logicalcpu)   # macOS
cmake --build build --config Debug                                     # Windows
```

Artifacts land in `build/SawPlugin_artefacts/Debug/{AU,VST3,Standalone}/`.

**Install on macOS (use `ditto`, not `cp` — preserves extended attributes):**
```bash
ditto "build/SawPlugin_artefacts/Debug/AU/MakeItASaw.component" \
      ~/Library/Audio/Plug-Ins/Components/MakeItASaw.component
codesign -s - --force ~/Library/Audio/Plug-Ins/Components/MakeItASaw.component

ditto "build/SawPlugin_artefacts/Debug/VST3/MakeItASaw.vst3" \
      ~/Library/Audio/Plug-Ins/VST3/MakeItASaw.vst3
codesign -s - --force ~/Library/Audio/Plug-Ins/VST3/MakeItASaw.vst3
```

---

## Known Limitations

- **Note onset**: the first cycle of a new note may sound slightly flat while the zero-crossing detector locks on. This is intrinsic to the ZCD approach and is most noticeable at low tempos or with slow attacks.
- **Crackling on high notes**: harmonics can cause false zero-crossing triggers even with TONE rolled off and DRIVE applied. This is most pronounced above the 12th fret on guitar or with instruments that have strong upper partials. Using DRIVE (20–40%) and rolling TONE to 2–4 kHz reduces it significantly.
- **Monophonic only**: chords or simultaneous pitches are not supported and will produce glitched output.
