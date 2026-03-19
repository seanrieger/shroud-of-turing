# Hardware Notice

## The Nocturne Alchemy Platform Hardware Is Proprietary (For Now, At Least)

The **Nocturne Alchemy Platform** hardware — including schematics, PCB layout, panel design, and all associated hardware design files — is the proprietary property of FlatSix Modular and is **not covered by the CC BY-NC-SA 4.0 license** that applies to this firmware.

This repository contains firmware source code only. You cannot manufacture or replicate the Nocturne Alchemy Platform hardware from the contents of this repository.

---

## What This Means for You

**If you own a Nocturne Alchemy Platform module:**  
You are free to flash this firmware (and any modifications you make) to your own hardware. The CC BY-NC-SA 4.0 license applies to your use of the firmware code.

**If you want to build derivative firmware:**  
You may modify and redistribute the firmware source code under CC BY-NC-SA 4.0. However, you may not replicate, manufacture, or sell hardware based on the Nocturne Alchemy Platform design.

**If you want to port this firmware to different hardware:**  
You are welcome to do so under the terms of CC BY-NC-SA 4.0. The firmware architecture is documented in `docs/DEVELOPER_GUIDE.md`. Note that the shared library files (`CalibrationMode`, `EEPROMHandling`) are tightly coupled to the Nocturne Alchemy Platform's hardware and EEPROM layout and would require significant rework for other platforms.

---

## Contact

For hardware-related enquiries, visit [flatsixmodular.com] or contact FlatSix Modular directly.

---

*FlatSix Modular — All hardware design rights reserved.*
