# Shroud of Turing v1.2.0 — Developer Guide

---

## Overview

This guide is for developers who want to understand, modify, or extend the Shroud of Turing firmware. It assumes basic familiarity with Arduino/C++ development and Eurorack modular synthesis concepts.

---

## Architecture

The firmware is a single `.ino` file with shared platform libraries. All logic lives in `ShroudOfTuring_v1_2_0.ino`. The shared libraries are compiled alongside it but must not be modified.

### Code Organization

```
setup()
└── Boot mode detection (2-second window)
    ├── Both octave buttons → Calibration mode
    ├── SHIFT button → CV Keyboard mode
    └── Default → Turing Machine mode

loop()
├── inCalibrationMode  → handleCalibrationMode()     [shared library]
├── inCVKeyboardMode   → handleCVKeyboardOperation()  [firmware]
└── default            → updateProbability()
                         handleButtonMatrix()
                         handleOctaveButtons()
                         handleClock()
```

### Data Flow (Turing Machine mode)

```
Clock rising edge
      ↓
handleClock()
      ↓
advanceTuringMachine()
      ↓
 ┌─────────────────────────────┐
 │ 1. Check reset pending      │
 │ 2. Apply rotation if queued │
 │ 3. Extract output bits      │
 │ 4. Apply clear/set if held  │
 │ 5. quantizeVoltage()        │
 │ 6. Output to DAC            │
 │ 7. Calculate feedback bit   │
 │ 8. Shift register left      │
 │ 9. Increment step counter   │
 └─────────────────────────────┘
```

---

## Key Functions

### `advanceTuringMachine()`

Core sequencer function, called on every rising clock edge. Handles reset, rotation, bit manipulation, CV output, and the shift register feedback loop.

### `updateProbability()`

Reads the potentiometer (0–1023), applies a 75/25 moving average filter, and sets `probabilityValue` and `currentPotMode`. The five pot modes are defined by threshold constants at the top of the file — adjust these to change the feel of the knob zones.

### `quantizeVoltage(byte cvBits)`

Converts an 8-bit shift register value to a calibrated output voltage. If a scale is active (`scaleNoteCount > 0`), snaps to the nearest scale note using `findNearestScaleNote()`. If no scale is active, outputs unquantized voltage proportional to `voltageRange`.

### `handleButtonMatrix()`

Scans the 3×4 matrix on every loop. Handles three distinct button contexts:

- **SHIFT mode** — sequence length, reset, clear/set bits, rotation
- **Octave Down long-hold** — scale save
- **Octave Up long-hold** — scale load / clear scale
- **Normal press** — add note to quantization scale

### `safeEEPROMWrite()` / `safeEEPROMRead()`

Address-validated EEPROM access. Rejects writes outside addresses 104–117 and double-checks against the protected calibration range (0–103) and the platform flag zone (200–201). Uses `EEPROM.update()` to minimise write cycles.

---

## Constants to Tune

All key thresholds are defined near the top of the file:

```cpp
// Pot mode boundaries (0–1023 range)
const int POT_DOUBLE_MAX   = 15;    // Below this = DOUBLE mode
const int POT_RANDOM_MIN   = 500;   // Centre of RANDOM zone
const int POT_RANDOM_MAX   = 510;
const int POT_LOCKED_MIN   = 1005;  // Above this = LOCKED mode

// SLIP zone boundaries
const int POT_SWEETSPOT_LOW_START  = 60;
const int POT_SWEETSPOT_LOW_END    = 420;
const int POT_SWEETSPOT_HIGH_START = 600;
const int POT_SWEETSPOT_HIGH_END   = 950;

// Timing
const unsigned long debounceDelayButtons    = 50;   // ms
const unsigned long shiftModeActivationTime = 150;  // ms hold for SHIFT/long-hold
```

---

## EEPROM Map

```
Address Range   Contents                       Status
─────────────   ──────────────────────────     ──────────────
0–3             Calibration signature          🔒 PROTECTED
4–101           Calibration data (49 notes)    🔒 PROTECTED
102–103         Integer format flag            🔒 PROTECTED
104–105         Scale storage signature        Shroud data
106–107         Scale slot D                   Shroud data
108–109         Scale slot E                   Shroud data
110–111         Scale slot F                   Shroud data
112–113         Scale slot G                   Shroud data
114–115         Scale slot A                   Shroud data
116–117         Scale slot B                   Shroud data
118–199         Available                       Free
200–201         Platform flag                  🔒 PROTECTED
202–1023        Available                       Free
```

Scales are stored as packed 16-bit values (one bit per chromatic note, bits 0–11). The signature `0xA5A5` at address 104 is checked before any load operation to prevent reading uninitialised EEPROM.

---

## Adding Features

### Adding a new SHIFT function

1. Choose an unused `noteIndex` in `handleButtonMatrix()` (the `switch (noteIndex)` block inside `if (shiftModeActive)`)
2. Add a `case` for it with your logic
3. Add a boolean flag if the action needs to persist beyond a single clock tick (like `clearBitButtonHeld`)

### Adding a new pot mode

1. Add a value to the `PotMode` enum
2. Add threshold constants
3. Add detection logic in `updateProbability()`
4. Add a `case` in the `switch (currentPotMode)` block

### Adding EEPROM storage

Use addresses 118–199 or 202–1023. Always use `safeEEPROMWrite()` / `safeEEPROMRead()` — or extend them to cover your new address range. Never write to 0–103 or 200–201.

---

## Memory

The Arduino Nano (ATmega328P) has 32KB flash and 2KB RAM. Keep dynamic memory under 60% for stable operation. Set `DEBUG_MODE false` for production — this saves approximately 400 bytes of RAM by eliminating Serial buffers and all debug strings.

Check memory after significant changes:

```
Sketch uses XXXXX bytes (XX%) of program storage space.
Global variables use XXXX bytes (XX%) of dynamic memory.
```

---

## Debugging

Set `DEBUG_MODE true` at the top of the `.ino` file and open Serial Monitor at 9600 baud. Debug output includes:

- Boot mode detection
- Step counter, output bits, voltage on every clock tick
- Quantization active indicator `[Q]`
- Scale add/clear operations
- EEPROM save/load operations with addresses and values
- Unsafe address rejection warnings

---

## Compiling

- **Board:** Arduino Nano
- **Processor:** ATmega328P (Old Bootloader for most modules - O on the Arduino, N on the Arduino means New Bootloader)
- **Required library:** Adafruit_MCP4725 (via Library Manager)
- All `.h` and `.cpp` files must be in the same sketch folder as the `.ino`

---

## Known Limitations

- No persistent storage of the Turing Machine pattern across power cycles (by design — patterns are intentionally volatile)
- Scale quantization uses nearest-neighbour lookup; there is no wrap-around pitch correction at octave boundaries
- CV Keyboard mode portamento mapping is backwards by design (higher pot value = more glide, but the internal mapping runs from high-to-low) — see `handlePotentiometerKeyboardMode()`
- `handlePlaybackMode.h/.cpp` is compiled but unused; it is a dependency of the shared library architecture and cannot be removed without IDE warnings on some versions

---

## License

Derivative firmware must be released under CC BY-NC-SA 4.0 with attribution to FlatSix Modular. The hardware design is proprietary and is not available for replication.

---

*Shroud of Turing v1.2.0 — FlatSix Modular*
