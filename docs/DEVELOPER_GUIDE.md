# Shroud of Turing v1.2.4 — Developer Guide

---

## Overview

This guide is for developers who want to understand, modify, or extend the Shroud of Turing firmware. It assumes basic familiarity with Arduino/C++ development and Eurorack modular synthesis concepts.

---

## Architecture

The firmware is a single `.ino` file with shared platform libraries. All logic lives in `ShroudOfTuring_v1_2_4_DEV.ino`. The shared libraries are compiled alongside it but must not be modified.

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
 ┌─────────────────────────────────────────┐
 │ 1. Check reset pending                  │
 │    └─ output noteArray[rotationOffset]  │
 │       stepCounter = 1, return           │
 │ 2. LOCKED / DOUBLE path:                │
 │    output noteArray[step + offset]      │
 │    apply clear/set → rebuildNoteArray() │
 │    increment step counter, return       │
 │ 3. RANDOM / VARIABLE path:              │
 │    extract output bits                  │
 │    apply clear/set if held              │
 │    quantizeVoltage() → DAC output       │
 │    calculate feedback bit               │
 │    shift register left                  │
 │    increment step counter               │
 └─────────────────────────────────────────┘
```

---

## Key Functions

### `advanceTuringMachine()`

Core sequencer function, called on every rising clock edge. Has two distinct output paths: locked/double mode outputs directly from the pre-computed `noteArray` using the current `rotationOffset`; random/variable mode uses the traditional bit-extraction and quantization path. Reset handling outputs from `noteArray[rotationOffset]` and sets `stepCounter = 1` to prevent a double-note on the following clock.

### `rebuildNoteArray()`

Pre-computes DAC output values for every step in the current locked or double-locked pattern and stores them in `noteArray[16]`. Mirrors the bit-extraction logic of `advanceTuringMachine()` exactly. Called automatically whenever the scale, step length, voltage range, or lock state changes. Resets `rotationOffset` to 0 since the underlying pattern has changed. Does nothing if neither `patternLocked` nor `patternDouble` is true.

### `applyRotation(int direction)`

Immediately shifts `rotationOffset` by one step in the given direction (+1 forward, -1 backward) with wraparound at the effective sequence length. Only fires if `noteArrayValid` is true and a stable pattern exists. Called directly from `handleButtonMatrix()` — no clock pulse required.

### `updateProbability()`

Reads the potentiometer (0–1023), applies a 75/25 moving average filter, and sets `probabilityValue` and `currentPotMode`. Calls `rebuildNoteArray()` the first time the pot enters LOCKED or DOUBLE mode. Invalidates `noteArrayValid` when either mode exits. Also implements pot pickup after a state load — the pot is ignored until it moves more than 30 ADC counts from its position at load time.

### `quantizeVoltage(byte cvBits)`

Converts an 8-bit shift register value to a calibrated output voltage. If a scale is active (`scaleNoteCount > 0`), snaps to the nearest scale note using `findNearestScaleNote()`. If no scale is active, outputs unquantized voltage proportional to `voltageRange`.

### `handleButtonMatrix()`

Scans the 3×4 matrix on every loop. Handles four distinct button contexts:

- **SHIFT mode** — sequence length, reset, clear/set bits, rotation (via `applyRotation()`)
- **Octave Down long-hold** — scale save (white keys) or full state save (black keys)
- **Octave Up long-hold** — scale load (white keys), full state load (black keys), or clear scale (C)
- **Normal press** — add note to quantization scale

### `saveStateToSlot(int slotAddress)` / `loadStateFromSlot(int slotAddress)`

Save and restore the complete module state to a 12-byte EEPROM slot. Saved fields: signature (0xB1B1), shift register, step length, rotation offset, voltage range, reserved byte, pot ADC snapshot, packed scale. Address safety is validated via `isStateAddressSafe()` before any EEPROM access. Load validates the signature and field ranges before applying, resets `stepCounter` to 0, calls `unpackScale()` (which triggers `rebuildNoteArray()`), and arms pot pickup.

### `safeEEPROMWrite()` / `safeEEPROMRead()`

Address-validated EEPROM access for scale slots (104–117). Rejects writes outside that range and double-checks against the protected calibration range (0–103) and the platform flag zone (200–201). Uses `EEPROM.update()` to minimise write cycles.

### `isStateAddressSafe(int address)`

Separate address guard for state slots (118–177). Independent of `isAddressSafe()` so scale slot safety is unaffected.

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
118–129         State slot 1 (C#)              Shroud data
130–141         State slot 2 (D#)              Shroud data
142–153         State slot 3 (F#)              Shroud data
154–165         State slot 4 (G#)              Shroud data
166–177         State slot 5 (A#)              Shroud data
178–199         Available                      Free
200–201         Platform flag                  🔒 PROTECTED
202–1023        Available                      Free
```

Scales are stored as packed 16-bit values (one bit per chromatic note, bits 0–11). The signature `0xA5A5` at address 104 is checked before any scale load operation to prevent reading uninitialised EEPROM.

State slots are 12 bytes each, laid out as follows:

```
Offset   Field            Type       Notes
──────   ───────────────  ─────────  ────────────────────────────
+0–1     Signature        uint16_t   0xB1B1 — validates slot is written
+2–3     Shift register   uint16_t   Raw 16-bit pattern
+4       Step length      uint8_t    Valid: 3,4,5,6,8,12,16
+5       Rotation offset  uint8_t    0–15 — chosen downbeat position
+6       Voltage range    uint8_t    1–4 octaves
+7       Reserved         uint8_t    0x00
+8–9     Pot snapshot     uint16_t   ADC value at save time (0–1023)
+10–11   Packed scale     uint16_t   Same format as scale slots
```

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

Use addresses 178–199 or 202–1023. Always use `safeEEPROMWrite()` / `safeEEPROMRead()` for scale-range data, or `isStateAddressSafe()` as a model for your own address guard. Never write to 0–103 or 200–201.

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
- Step counter, output bits, voltage on every clock tick (random/variable mode)
- Step counter, rotation offset, output index, DAC value on every clock tick (locked/double mode)
- Note array rebuild events with full step listing
- Rotation offset changes
- Quantization active indicator `[Q]`
- Scale add/clear operations
- EEPROM save/load operations with addresses and values
- Unsafe address rejection warnings
- State save/load with all field values
- Pot pickup arm and release events

---

## Compiling

- **Board:** Arduino Nano
- **Processor:** ATmega328P (Old Bootloader for most modules - O on the Arduino, N on the Arduino means New Bootloader)
- **Required library:** Adafruit_MCP4725 (via Library Manager)
- All `.h` and `.cpp` files must be in the same sketch folder as the `.ino`

---

## Known Limitations

- No persistent storage of the Turing Machine pattern across power cycles (by design — patterns are intentionally volatile). Use State Save to explicitly persist a pattern.
- Scale quantization uses nearest-neighbour lookup; there is no wrap-around pitch correction at octave boundaries
- Rotation is only available in LOCKED or DOUBLE mode — it has no effect in random or variable probability modes
- The note array (`noteArray[16]`) stores a maximum of 16 steps. In DOUBLE mode with step lengths of 9–16, the effective length is capped at 16
- CV Keyboard mode portamento mapping is backwards by design (higher pot value = more glide, but the internal mapping runs from high-to-low) — see `handlePotentiometerKeyboardMode()`
- `handlePlaybackMode.h/.cpp` is compiled but unused; it is a dependency of the shared library architecture and cannot be removed without IDE warnings on some versions

---

## License

Derivative firmware must be released under CC BY-NC-SA 4.0 with attribution to FlatSix Modular. The hardware design is proprietary and is not available for replication.

---

*Shroud of Turing v1.2.4 — FlatSix Modular*
