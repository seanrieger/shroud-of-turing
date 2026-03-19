# Changelog

All notable changes to the Shroud of Turing firmware are documented here.

---

## v1.2.0 — January 2026

### Added

- **Scale save/recall** — Save up to 6 custom quantization scales to EEPROM, persistent across power cycles. Long hold Octave Down + note button to save; Long hold Octave Up + note button to load.
- **CV Keyboard mode** — Hold SHIFT (High C) during boot to enter a direct-play CV keyboard mode with gate output and portamento control via potentiometer.
- **EEPROM address validation** — All scale save/load operations include bounds checking and signature verification to protect calibration data from accidental corruption.

### Fixed

- Calibration mode now initialises correctly on boot when both octave buttons are held.
- Pattern now correctly repeats based on selected sequence length in LOCKED mode.
- SHIFT button (High C) debouncing corrected for reliable operation.
- Quantization now uses calibration data for accurate 1V/octave output.

### Improved

- Pattern rotation (forward/backward) via SHIFT + G# / A#.
- Clear bit (SHIFT + D#) and Set bit (SHIFT + F#) functions for direct pattern editing.
- EEPROM.update() used throughout to reduce EEPROM wear.

---

## v1.1.0 — November 2025

### Added

- **Musical quantization** — Press note buttons to build a live scale; CV output snaps to those notes.
- Pattern reset for both LOCKED and DOUBLE modes (SHIFT + C#).
- Pattern rotation forward and backward.
- Clear bit and Set bit functions for manual pattern manipulation.
- Memory optimisation via `DEBUG_MODE` compile flag (~400 bytes RAM saved when false).

---

## v1.0.x — October 2025

Initial development series. Core Turing Machine functionality:

- 16-bit shift register with probability control
- Variable sequence length (3–16 steps)
- DOUBLE mode (2× inverted pattern)
- LOCKED mode (zero-probability loop)
- Extended SLIP zones (1–15% probability sweet spots)
- Calibrated CV output (0–4V, 1V/octave)
- Clock input on pin 11 (rising edge detection)

---

*Shroud of Turing — FlatSix Modular*
