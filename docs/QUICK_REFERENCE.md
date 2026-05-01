# Shroud of Turing v1.2.4 — Quick Reference

---

## Boot Modes

Hold button during power-on (2-second window):

| Hold                | Mode                     |
| ------------------- | ------------------------ |
| Nothing             | Turing Machine (default) |
| SHIFT (High C)      | CV Keyboard              |
| Both Octave buttons | Calibration              |

---

## Potentiometer

```
7 o'clock ────────── 12 o'clock ────────── 5 o'clock
 DOUBLE      SLIP      RANDOM      SLIP      LOCKED
```

| Position     | Behaviour                                        |
| ------------ | ------------------------------------------------ |
| 7 o'clock    | DOUBLE — plays pattern then inverted (2× length) |
| 8–11 o'clock | SLIP — pattern slowly evolves                    |
| 12 o'clock   | RANDOM — total chaos                             |
| 1–4 o'clock  | SLIP — pattern slowly evolves                    |
| 5 o'clock    | LOCKED — perfect repeating loop                  |

---

## Normal Button Presses

| Button                    | Action                               |
| ------------------------- | ------------------------------------ |
| Any note button           | Add note to quantization scale       |
| Octave Up (short press)   | Increase voltage range (1→2→3→4 oct) |
| Octave Down (short press) | Decrease voltage range (4→3→2→1 oct) |

---

## SHIFT Mode (hold High C ≥150ms, LED lights up)

| SHIFT + | Action                                        |
| ------- | --------------------------------------------- |
| C       | Sequence length → 3 steps                     |
| C#      | Reset to chosen downbeat (LOCKED/DOUBLE only) |
| D       | Sequence length → 4 steps                     |
| D#      | Clear current bit (hold to continue clearing) |
| E       | Sequence length → 5 steps                     |
| F       | Sequence length → 6 steps                     |
| F#      | Set current bit (hold to continue setting)    |
| G       | Sequence length → 8 steps (default)           |
| G#      | Rotate sequence backward one step             |
| A       | Sequence length → 12 steps                    |
| A#      | Rotate sequence forward one step              |
| B       | Sequence length → 16 steps                    |

> **Rotation** shifts which note plays first without changing the underlying pattern.
> Only active in LOCKED or DOUBLE mode. Immediate — no clock pulse required.
> Rotation is preserved when you save state and restored when you load it.

---

## Scale Save / Recall

| Action                                         | Result                     |
| ---------------------------------------------- | -------------------------- |
| Long hold Octave Down (150ms+), then press D–B | Save current scale to slot |
| Long hold Octave Up (150ms+), then press D–B   | Load scale from slot       |
| Long hold Octave Up (150ms+), then press C     | Clear current scale        |

Slots: **D** = 1, **E** = 2, **F** = 3, **G** = 4, **A** = 5, **B** = 6

---

## State Save / Recall

Saves and restores the complete module state: shift register pattern, sequence
length, voltage range, scale, pot value, and rotation offset.

| Action                                          | Result                      |
| ----------------------------------------------- | --------------------------- |
| Long hold Octave Down (150ms+), then press C#–A# | Save full state to slot     |
| Long hold Octave Up (150ms+), then press C#–A#  | Load full state from slot   |

| Black Key | Slot |
| --------- | ---- |
| C#        | 1    |
| D#        | 2    |
| F#        | 3    |
| G#        | 4    |
| A#        | 5    |

> **After a load**, the pot is ignored until it moves ~30 ADC counts from its
> position at load time. This prevents the physical pot position from
> immediately overwriting the loaded probability value.
>
> State slots are independent of scale slots — loading a state overwrites the
> active scale in RAM but never touches the saved scale slots in EEPROM.

---

## CV Keyboard Mode

*(Enter by holding SHIFT/High C on boot)*

| Control          | Action                               |
| ---------------- | ------------------------------------ |
| Note buttons     | Play note (calibrated CV out)        |
| High C button    | Play high C                          |
| Octave Up / Down | Shift keyboard range (1–4 oct)       |
| Potentiometer    | Portamento amount (7 o'clock = none) |
| Clock jack       | Gate output (HIGH when key held)     |

---

## Calibration Mode

*(Enter by holding both Octave buttons on boot)*

| Control                | Action                            |
| ---------------------- | --------------------------------- |
| Octave Up / Down       | Shift octave range                |
| Note button            | Select note to calibrate          |
| High C + Potentiometer | Fine-tune selected note voltage   |
| Hold Octave Down 8 sec | Reset all calibration to defaults |

---

*Shroud of Turing v1.2.4 — FlatSix Modular*
