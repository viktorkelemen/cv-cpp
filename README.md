# CV Sender JUCE Example

This minimal JUCE app exposes a GUI that lets you pick your audio device and
toggle a constant CV-like DC output. It is meant as a starting point for
driving modular gear from a DC-coupled interface.

## Prerequisites

- A local JUCE checkout (6.1+ recommended) with the JUCE CMake API available.
- CMake 3.15 or newer and a C++17 toolchain (Xcode, clang, MSVC, etc.).
- An audio interface with DC-coupled outputs if you intend to feed actual CV.

## Configure & Build

```bash
cmake -S . -B build -DJUCE_DIR=/path/to/JUCE
cmake --build build
```

On macOS the resulting app bundle appears at `build/cv_sender_app_artefacts/Debug/CV Sender.app`.

## How it Works

- The main window embeds JUCE's `AudioDeviceSelectorComponent`, so you can
  switch between any audio devices the system exposes.
- Toggling the **Send CV** button pushes a constant `+1.0f` value through every
  active output channel in the audio callback, mimicking a high CV level.
- Stop the stream or toggle the button off to drop the output back to `0.0f`.
- Grid cells now show their note name, and the sidebar lets you route each of up to
  eight hardware outputs (e.g. an ES-8) to the sequencer's pitch lane, gate pulses (~8 V),
  clock pulse, or manual CV source with per-voice calibration offsets.

Extend this by replacing the constant with envelopes, LFO shapes, or other DSP
blocks, and by exposing more controls in the GUI.
