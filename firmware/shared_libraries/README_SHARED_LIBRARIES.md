# Shared Platform Libraries — DO NOT MODIFY

This folder contains two library files that are **shared across all Nocturne Alchemy Platform firmwares**:

```
CalibrationMode.h / CalibrationMode.cpp
EEPROMHandling.h  / EEPROMHandling.cpp
```

They are included here solely so the Shroud of Turing firmware compiles correctly. **They are not part of the open-source release and must be treated as read-only.**

---

## Why You Must Not Modify These Files

### The Calibration Data Problem

Every Nocturne Alchemy Platform module is individually hand-calibrated at the "factory" - (yep, that's me. I hand-calibrate every one of these that goes out.) That calibration data — unique to your specific hardware — is stored in EEPROM addresses **0–103**:

```
EEPROM Address Range   Contents
────────────────────   ────────────────────────────────────────
0–3                    Calibration signature (validates data)
4–101                  Calibration values for 49 notes (floats)
102–103                Integer format flag
```

`EEPROMHandling.cpp` reads and writes this region. `CalibrationMode.cpp` uses it to tune the CV output to accurate 1V/octave voltages.

**If you modify these files and upload broken code, you risk overwriting or corrupting your calibration data.**

### What Corrupted Calibration Looks Like

- CV output is out of tune and cannot be corrected by ear
- Voltages are incorrect across all notes
- The module appears to work but outputs wrong pitches or 0 volts for all pitches

### How Hard Is It to Fix?

Okay, so it's not the end of the world, but recalibration requires a precision multimeter and manually re-entering calibration values for all 49 notes through the calibration interface. It is time-consuming (if you don't do it often) and requires test equipment. **There is no "undo."** But if you release an open-source firmware that corrupts people's calibration and they start complaining to me, I will hunt you down. (Kidding... maybe)  All that being said, here's how to calibrate a module by hand: [How to calibrate a Nocturne Alchemy Platform module (YouTube)](https://youtu.be/dOwi5h5uOKQ?si=2P_2r_FoMVRRpi_h)

---

## What You Can Safely Change

The Shroud of Turing firmware (`ShroudOfTuring_v1_2_0.ino`) is the file intended for modification. It calls into these libraries but does not define them.

The safe EEPROM address range for the Shroud of Turing's own data is **104–117** (scale save slots). This is explicitly mapped in the firmware and protected from writing to the calibration region by address validation checks.

```
EEPROM Address Range   Contents                    Status
────────────────────   ─────────────────────────   ──────────────
0–103                  Calibration data            🔒 PROTECTED
104–117                Shroud scale save slots      Safe to use
118–199                Available                    Safe to use
200–201                Platform flag (unknown)     🔒 PROTECTED
202–1023               Available                    Safe to use
```

---

## Why Are These Files in the Repo at All?

The Arduino IDE requires all files referenced by `#include` to be present in the same sketch folder to compile. These files must be here for the build to work.

If you are building a derivative firmware, you will need these files from your own Nocturne Alchemy Platform module's firmware bundle. Do not modify them — copy them as-is.

---

## Summary

| File                        | Modify? | Why                              |
| --------------------------- | ------- | -------------------------------- |
| `CalibrationMode.h/.cpp`    | Never   | Manages factory calibration data |
| `EEPROMHandling.h/.cpp`     | Never   | Reads/writes calibration EEPROM  |
| `ShroudOfTuring_v1_2_0.ino` | Yes     | This is the firmware — go for it |

---

*Shroud of Turing v1.2.0 — FlatSix Modular*  
*Hardware design is proprietary and not covered by the CC BY-NC-SA 4.0 license.*
