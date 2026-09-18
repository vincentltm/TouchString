# This is TouchString(s)

## Quick Install
Download the [binary file](https://github.com/Synthux-Academy/TouchString/releases/latest/download/TouchString.bin) and flash using the [Daisy Seed web programmer](https://electro-smith.github.io/Programmer/)

## Controls

<img src="touch.jpeg" width="300"/>

### Pads
- **P00** (hold) - Octave Down (-1 oct) | Hold both P00 + P02 for Sub-bass (-2 oct)
- **P01** - String Dampen Pad / Acoustic Choke (squeeze to damp ringing strings, tap to choke)
- **P02** (hold) - Octave Up (+1 oct)
- **P03...P09** - Note pads (strings 1 to 7)
- **P10** - "TO" modifier | Hold to enable tempo control and alternative knob functions:
  - P10 + P00 - Tempo down
  - P10 + P02 - Tempo up
- **P11** - "CH" modifier | Hold to enable scale selection and voice modes:
  - P11 + P00 - Previous scale
  - P11 + P02 - Next scale
  - P11 + P01 - Poly / Mono toggle (LED double-blink = Mono, single blink = Poly)

### Knobs (clockwise)
- S30 **Brightness** | Controls filter brightness/cutoff frequency
- S31 **Pitch** | Root note tuning across $\pm 1$ octave (25 chromatic semitones, center = unison root)
- S32 **Timbre** | Sound structure/harmonic content
- S33 **Density** | Pattern density/complexity
- S33 + TO (pad 10) **Pattern Shift**
- S34 **Notes** | Humanization of note timing/variation
- S35 **String Chance** | Probability of string triggering
- S35 + TO (pad 10) **Reverb Mix**

### Faders
- S36 (left) **Drive** | Lower range attenuates volume to quiet; upper range increases overdrive
- S36 + TO (pad 10) **Input Volume** | External audio input gain for sympathetic string resonance (Rings mode)
- S37 (right) **Damp** | String damping/decay time

### Switches

- **1** (left) - Exciter Mode
  - **Up**: Pluck (velocity-sensitive)
  - **Center**: Pluck + Bow on hold (continuous pressure)
  - **Down**: Bow (continuous excitation modulated by pressure)

- **A** (right) - Arpeggiator
  - **Down**: Arpeggiator Off
  - **Center**: Arpeggiator On
  - **Up**: Arpeggiator Latched

### Polyphony & Expression
- **Poly / Mono Modes**: Toggle with P11 + P01. In Poly mode, all 7 string voices run concurrently in stereo. In Mono mode, notes have last-note priority with centered stereo panning, smooth legato pitch gliding (~35 ms portamento) in Bow mode, and clean single-string damping in Pluck mode.
- **Octave Hold Drone**: Octave shift is locked per voice when triggered. Holding P00 drops the current note by 1 octave, while releasing P00 allows subsequent plucked/bowed notes to play at standard pitch or +1 octave (P02). This enables holding a continuous bowed bass drone while playing higher melody notes!
- **String Dampen / Acoustic Choke (P01)**: Squeezing P01 introduces strong acoustic damping across all strings to simulate palm-muting. Tapping P01 quickly silences ringing resonance.
- **Velocity Sensitivity**: Pluck strike force affects attack volume and brightness.
- **Aftertouch**: In Pluck mode, micro-rocking or squeezing a held pad bends pitch slightly (+25 cents) for finger vibrato. In Bow mode, pressure modulates bow friction and timbre.

### LED Indicator

The onboard LED is lit when latch is active.

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
