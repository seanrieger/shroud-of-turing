# Changelog

All notable changes to the Shroud of Turing firmware are documented here.

---

## v1.2.7 — May 2026

### Added

- **Voltage window shift** — Hold SHIFT and press Octave Up or Octave Down to
  slide the output voltage window in 1V steps. The window is defined by a
  `windowOffset` (0–3V) paired with the existing `voltageRange` (1–4 octaves).
  A 1-octave window at offset 0 = C0–C1; at offset 1 = C1–C2, and so on.
  Quantized output shifts by whole octaves through `calibrationValues[]`;
  unquantized output shifts linearly. Window stops at boundaries (hard stop
  at 0V and 4V — no compression logic).
- **Push-down on range expansion** — When the voltage range is increased via
  a normal Octave Up press, `windowOffset` is automatically clamped to
  `min(windowOffset, 4 - voltageRange)` to keep the window top within 4V.
- **Window offset persisted in state save/load** — `windowOffset` is stored
  in the previously reserved byte (slot offset 7). State signature bumped to
  `0xB3B3` to cleanly invalidate older slots. On load, offset is validated
  (`winOffset <= 3`) and clamped to enforce the invariant.
- **CV Keyboard mode unaffected** — Window shift is intentionally disabled
  in CV Keyboard mode; it retains its own independent octave range logic.

---

## v1.2.6 — May 2026

### Added

- **Single note removal from scale** — Long-hold any note button for 800ms
  to remove that note from the active quantization scale. Previously the only
  way to remove notes was to clear the entire scale.
- **Press → release architecture for note input** — `addNoteToScale()` now
  fires on button release rather than press. This allows the same button to
  distinguish between a short press (add) and a long hold (remove) without
  ambiguity or accidental double-actions.
- `NOTE_REMOVE_HOLD_TIME = 800` ms named constant for easy retuning.
- Long-hold removal is blocked when Shift or either Octave long-hold is
  active, preventing interference with save/load/scale-slot operations.

---

## v1.2.5 — May 2026

### Added

- **Slew/portamento for Turing Machine mode** — Hold SHIFT, then adjust the
  potentiometer to set glide amount. Fully left (7 o'clock) = instant, fully
  right (5 o'clock) = maximum glide. Uses an exponential taper curve for
  perceptually linear response. Slew is applied via 500µs interpolation timer.
- **Shift+pot context switching** — When SHIFT is held, the pot switches from
  probability to slew control. Both directions use movement-based pickup
  protection (30 ADC count threshold) so the value cannot jump accidentally.
- **Slew persisted in state save/load** — State slots now store the slew
  setting alongside pattern, scale, length, range, and rotation. Slew is
  restored exactly on load without requiring any pot pickup — it stays frozen
  until the user next holds SHIFT.
- **State slot layout expanded** — Slots grow from 12 to 14 bytes to
  accommodate the slew snapshot. Slot addresses updated (118–187). EEPROM
  signature bumped to `0xB2B2` to cleanly invalidate v1.2.4 slots.

### Fixed

- **Probability pickup after SHIFT release** — Previously, releasing SHIFT
  caused the pot to immediately adopt the physical position as the new
  probability value, ignoring the pickup guard. Root cause: the 75/25 moving
  average filter was not updated during the SHIFT-held period, so `lastPotValue`
  went stale. On SHIFT release, the first filtered read blended the stale value
  with the live raw read, producing a large phantom delta that instantly cleared
  the pickup flag. Fixed by always running the filter (even during SHIFT-held
  early return) and comparing raw ADC against raw baseline in both pickup checks.
- **Save captures effective probability, not physical pot** — If a pickup guard
  is active at save time (after SHIFT release or after a state load), the saved
  pot value now reflects the probability the user is actually hearing rather than
  the physical knob position. Priority: `probPotPickupRequired` →
  `frozenPotValue`; `potPickupRequired` → `savedPotValue`; otherwise live read.

---

## v1.2.4 — February 2026

### Added

- **State save/load** — Save and restore the complete module state (shift
  register, sequence length, voltage range, scale, pot value, rotation offset)
  to one of 5 EEPROM slots. Long hold Octave Down + black key to save; Long
  hold Octave Up + black key to load. Slots use black keys C#/D#/F#/G#/A#.
- **Pot pickup after state load** — Physical pot is ignored on load until it
  moves deliberately, preventing the loaded probability from being immediately
  overwritten by the knob's current position.
- **Pattern rotation persisted** — Rotation offset is saved and restored with
  full state.

---

## v1.2.0 — January 2026

### Added

- **Scale save/recall** — Save up to 6 custom quantization scales to EEPROM,
  persistent across power cycles. Long hold Octave Down + note button to save;
  Long hold Octave Up + note button to load.
- **CV Keyboard mode** — Hold SHIFT (High C) during boot to enter a direct-play
  CV keyboard mode with gate output and portamento control via potentiometer.
- **EEPROM address validation** — All scale save/load operations include bounds
  checking and signature verification to protect calibration data from accidental
  corruption.

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
