# This is TouchString(s)

Polyphonic physical modeling string synthesizer for the Synthux Touch synthesizer and Daisy Seed.

## Quick Install
Flash the precompiled [**TouchString.bin**](https://raw.githubusercontent.com/vincentltm/TouchString/main/TouchString.bin) directly using the [Daisy Seed web programmer](https://flash.daisy.audio/).

## Controls

<img src="touch.jpeg" width="300"/>

### Pads
- **P00** - Keyboard Octave Down (steps down up to -2 octaves) | Tap both P00 + P02 to reset to unison (0)
- **P01** - String Dampen / Palm-Mute Choke (squeeze to damp ringing strings, tap to mute)
- **P02** - Keyboard Octave Up (steps up up to +2 octaves) | Tap both P00 + P02 to reset to unison (0)
- **P03...P09** - Note pads (strings 1 to 7)
- **P10 (TO)** - Modifier | Hold for secondary functions:
  - P10 + P00 - Tempo down
  - P10 + P02 - Tempo up
  - P10 + S33 - Pattern Shift
  - P10 + S35 - Reverb Mix
  - P10 + S36 - External Audio Input Volume
- **P11 (CH)** - Modifier | Hold for scale and voice mode selection:
  - P11 + P00 - Previous scale (cycles through all 7 scales)
  - P11 + P02 - Next scale (cycles through all 7 scales)
  - P11 + P03...P09 - Direct scale select (P03: Amara, P04: Oxalis, P05: Pigmy, P06: Major, P07: Natural Minor, P08: Minor Pentatonic, P09: Major Pentatonic)
  - P11 + P01 - Poly / Mono toggle (LED double-blink = Mono, single blink = Poly)

### Knobs (clockwise)
- S30 **Brightness** | Exciter filter cutoff & string brightness
- S31 **Pitch** | Global tuning across ±1 octave (center = unison)
- S32 **Timbre** | String structure / harmonic content
- S33 **Density** | Arpeggiator pattern density (Hold TO: Pattern Shift)
- S34 **Notes** | Arpeggiator note humanization
- S35 **String Chance** | Trigger probability (Hold TO: Reverb Mix)

### Faders
- S36 (left) **Drive** | Master volume & overdrive (Hold TO: External Input Gain)
- S37 (right) **Damp** | String decay sustain time / *bariolage* ring

### Switches
- **Left Switch (S09/S10) — Exciter Mode**
  - **Up**: Pluck (velocity-sensitive, whisper-quiet to snappy)
  - **Center**: Pluck + Bow on hold (squeeze pad to swell into bowed sustain)
  - **Down**: Bow (continuous stick-slip bowing & scratch sensitivity)

- **Right Switch (S07/S08) — Arpeggiator**
  - **Up**: Latched
  - **Center**: On (momentary)
  - **Down**: Off

### Polyphony & Expression
- **Poly / Mono Modes**: Toggle with P11 + P01. In Poly mode, all 7 strings run in stereo. In Mono mode, notes have last-note priority with centered stereo panning and smooth portamento.
- **Bowed Arpeggiator**: In Bow mode, Mono plays a seamless, singing slurred legato line across notes. In Poly mode, strings ring out together in harmonic *bariolage*.
- **Keyboard Octaves & Bass Drones**: Tapping P00 or P02 shifts the keyboard for new notes without interrupting currently held notes. You can hold a low bass drone with one hand, shift octaves, and pluck high melodies over it.
- **String Dampen (P01)**: Squeeze P01 for acoustic palm-muting across all strings, or tap it to silence resonance.
- **Velocity Sensitivity**: Responsive kinetic tracking from whisper-quiet soft plucks to bright, punchy fortissimo.
- **Aftertouch**: In Pluck mode, micro-rocking a held pad bends pitch slightly for acoustic finger vibrato. In Bow mode, pad pressure continuously modulates bow friction and timbre.

### Scales
Hold **P11 (CH)** and tap a note pad to jump directly to any scale, or use **P00 / P02** to cycle:
- **P03 — Amara**: Celtic minor handpan tuning (`C3, G3, Bb3, C4, D4, Eb4, G4`)
- **P04 — Oxalis**: Folk major handpan tuning (`C3, E3, F3, G3, A3, C4, F4`)
- **P05 — Pigmy**: Dorian Pygmy minor tuning (`C3, D3, Eb3, G3, Bb3, C4, Eb4`)
- **P06 — Major**: Diatonic Major (`C3, D3, E3, F3, G3, A3, C4`)
- **P07 — Natural Minor**: Diatonic Minor (`C3, D3, Eb3, F3, G3, Ab3, C4`)
- **P08 — Minor Pentatonic**: Blues & folk (`C3, Eb3, F3, G3, Bb3, C4, Eb4`)
- **P09 — Major Pentatonic**: Bright & sweet (`C3, D3, E3, G3, A3, C4, D4`)

### LED Indicator
The onboard LED shows arpeggiator latch status, confirms scale changes (quick flash), and confirms Poly/Mono toggling (double blink = Mono, single blink = Poly).

## Project Structure
```
TouchString/
├── TouchString.cpp      # Main application entry point
├── Makefile             # Build configuration
|-- common               # Configuration and utilities
├── string/              # Instrument core
├── touch/               # Simple Touch wrapper (pads, knobs, switches)
├── ui/                  # UI connecting instrument core with touch wrapper
└── lib/                 # Libraries
    ├── libDaisy/        # Daisy hardware abstraction
    └── DaisySP/         # DSP library
```

## Project Setup
```shell
$ git clone --recurse-submodules https://github.com/Synthux-Academy/TouchString.git
$ make libs -j8
$ make clean; make -j8
```

## Configuration
Edit [config.h](https://github.com/Synthux-Academy/TouchString/blob/main/common/config.h) to customize the instrument.
