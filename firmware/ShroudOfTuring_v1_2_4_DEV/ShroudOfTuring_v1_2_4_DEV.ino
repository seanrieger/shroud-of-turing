// =============================================================================
// Shroud of Turing v1.2.4
// Turing Machine MKII-Style Random Sequencer with Musical Quantization
// For the Nocturne Alchemy Platform (Arduino Nano)
//
// Copyright (c) 2026 FlatSix Modular
// Licensed under CC BY-NC-SA 4.0
// https://creativecommons.org/licenses/by-nc-sa/4.0/
//
// You may use, modify, and distribute this firmware for non-commercial
// purposes, provided you credit FlatSix Modular and share any
// modifications under the same license.
//
// The Nocturne Alchemy Platform hardware design is NOT covered by
// this license and remains proprietary.
//
// Original Turing Machine concept by Tom Whitwell / Music Thing Modular
// =============================================================================
//
// *** THREE OPERATING MODES ***
// 1. TURING MACHINE MODE (default boot)
//    - Random looping sequencer with quantization
//    - Potentiometer controls probability
//    - Clock input advances sequence
// 2. CV KEYBOARD MODE (hold SHIFT/High C during boot)
//    - Direct note playing from button matrix
//    - Gate output when keys pressed
//    - Potentiometer controls portamento/glide
// 3. CALIBRATION MODE (hold both Octave buttons during boot)
//    - Calibrate CV output for accurate tuning
//    - Shared across all modes
//
// *** SHARED LIBRARY WARNING ***
// CalibrationMode.h/.cpp and EEPROMHandling.h/.cpp are shared platform
// libraries used by ALL Nocturne Alchemy Platform firmwares. DO NOT MODIFY
// these files. They manage calibration data stored in EEPROM addresses 0-103.
// Modifying and re-uploading these files can corrupt your calibration data,
// requiring full recalibration with external equipment.
// =============================================================================
#include <EEPROM.h>
#include "CalibrationMode.h"
#include "EEPROMHandling.h"

// *** SET TO false FOR PRODUCTION (saves ~400 bytes RAM) ***
#define DEBUG_MODE false

Adafruit_MCP4725 dac;

const int rows = 3;
const int cols = 4;
byte rowPins[rows] = { 2, 3, 4 };
byte colPins[cols] = { 5, 6, 7, 8 };

const int octaveUpPin = 9;
const int octaveDownPin = 10;
const int triggerPin = 11;
const int highCButton = 12;
const int potentiometerPin = A0;

const unsigned long debounceDelayButtons = 50;
const unsigned long shiftModeActivationTime = 150;

// Potentiometer thresholds
const int POT_DOUBLE_MAX = 15;
const int POT_DOUBLE_EXIT = 60;
const int POT_RANDOM_MIN = 500;
const int POT_RANDOM_MAX = 510;
const int POT_LOCKED_MIN = 1005;
const int POT_LOCKED_EXIT = 950;

const int POT_SWEETSPOT_LOW_START = 60;
const int POT_SWEETSPOT_LOW_END = 420;
const int POT_SWEETSPOT_HIGH_START = 600;
const int POT_SWEETSPOT_HIGH_END = 950;

enum PotMode { POT_MODE_DOUBLE, POT_MODE_VARIABLE_LOW, POT_MODE_RANDOM, POT_MODE_VARIABLE_HIGH, POT_MODE_LOCKED };
PotMode currentPotMode = POT_MODE_RANDOM;

extern bool testMode;

// Core variables
unsigned int shiftRegister = 0;
unsigned int lockedPattern = 0;
bool patternLocked = false;
unsigned int doublePattern = 0;
bool patternDouble = false;
int currentStepLength = 8;
int stepCounter = 0;
bool doubleLengthMode = false;
float probabilityValue = 0.5;
int voltageRange = 1;

bool shiftModeActive = false;
unsigned long shiftButtonPressTime = 0;
bool turingLastButtonState[rows][cols];
unsigned long lastDebounceTime[rows][cols];
bool lastShiftButtonState = false;
bool lastOctaveUpState = false;
bool lastOctaveDownState = false;
unsigned long lastOctaveDebounceTime = 0;
bool lastClockState = LOW;
bool resetPending = false;
bool clearBitButtonHeld = false;
bool setBitButtonHeld = false;

// *** Note array rotation (v1.2.4) ***
// Pre-computed DAC output values for locked/double modes.
// Rotation operates on this array directly — shift register is untouched.
uint16_t noteArray[16];       // 32 bytes — DAC values for each step
int rotationOffset = 0;       // current rotation position (0 to stepLength-1)
bool noteArrayValid = false;  // must rebuild before use

// *** Quantization variables ***
bool scaleNotes[12] = {false};
int scaleNoteCount = 0;

// *** CV Keyboard Mode variables ***
bool inCVKeyboardMode = false;
bool anyKeyPressed = false;
bool lastAnyKeyPressed = false;
unsigned long lastKeyboardDebounceTime[rows][cols];
bool lastKeyboardButtonState[rows][cols];
unsigned long lastHighCDebounceTime = 0;
bool lastHighCState = false;
const unsigned long keyboardDebounceDelay = 25;  // Fast response for keyboard

// Portamento (glide) variables
float keyboardCurrentVoltage = 0.0;
float keyboardTargetVoltage = 0.0;
float portamentoRate = 0.0;  // 0.0 = instant, higher = slower glide
unsigned long lastPortamentoUpdate = 0;
unsigned long octaveUpPressTime = 0;
bool octaveUpLongHoldActive = false;
bool modeChangeOccurred = false;

// *** Scale save/recall with EEPROM SAFEGUARDS ***

// EEPROM address map following Nocturne Alchemy Platform standard:
// 0-3: Calibration signature
// 4-101: Calibration data (49 notes x 4 bytes)
// 102-103: Integer flag
// 104-139: ARP BUFFER SPACE (36 bytes) - WE USE 104-117 (14 bytes)
// 200-201: PROTECTED - Must skip

const int SCALE_EEPROM_START = 104;
const int SCALE_EEPROM_END = 117;
const int SCALE_SIGNATURE_ADDR = 104;
const uint16_t SCALE_SIGNATURE = 0xA5A5;
const int SCALE_SLOT_D_ADDR = 106;
const int SCALE_SLOT_E_ADDR = 108;
const int SCALE_SLOT_F_ADDR = 110;
const int SCALE_SLOT_G_ADDR = 112;
const int SCALE_SLOT_A_ADDR = 114;
const int SCALE_SLOT_B_ADDR = 116;

// *** Save State Slots (v1.2.4) ***
// 5 slots x 12 bytes each, addresses 118-177
// Slot 1 (C#): 118-129
// Slot 2 (D#): 130-141
// Slot 3 (F#): 142-153
// Slot 4 (G#): 154-165
// Slot 5 (A#): 166-177
const int STATE_SLOT_1_ADDR = 118;
const int STATE_SLOT_2_ADDR = 130;
const int STATE_SLOT_3_ADDR = 142;
const int STATE_SLOT_4_ADDR = 154;
const int STATE_SLOT_5_ADDR = 166;
const int STATE_SLOT_END    = 177;
const uint16_t STATE_SIGNATURE = 0xB1B1;

// CRITICAL: Protected address ranges (DO NOT WRITE!)
const int PROTECTED_CALIBRATION_START = 0;
const int PROTECTED_CALIBRATION_END = 103;
const int PROTECTED_FLAG_START = 200;
const int PROTECTED_FLAG_END = 201;

unsigned long octaveDownPressTime = 0;
bool octaveDownLongHoldActive = false;

// *** Save/Load State variables (v1.2.4) ***
bool potPickupRequired = false;   // true after a state load until pot moves
int  potBaselineOnLoad = 0;       // filtered pot reading captured at load time
int  savedPotValue = 512;         // pot ADC value stored in the state slot

// *** EEPROM Safety Functions ***

bool isAddressSafe(int address) {
    if (address < SCALE_EEPROM_START || address > SCALE_EEPROM_END) {
        #if DEBUG_MODE
            Serial.print(F("!!! UNSAFE ADDR: "));
            Serial.println(address);
        #endif
        return false;
    }
    if (address >= PROTECTED_CALIBRATION_START && address <= PROTECTED_CALIBRATION_END) {
        #if DEBUG_MODE
            Serial.println(F("!!! CALIBRATION PROTECTED"));
        #endif
        return false;
    }
    if (address >= PROTECTED_FLAG_START && address <= PROTECTED_FLAG_END) {
        #if DEBUG_MODE
            Serial.println(F("!!! FLAG PROTECTED"));
        #endif
        return false;
    }
    return true;
}

bool safeEEPROMWrite(int address, uint16_t value) {
    if (!isAddressSafe(address)) return false;
    if (!isAddressSafe(address + 1)) return false;
    EEPROM.update(address, lowByte(value));
    EEPROM.update(address + 1, highByte(value));
    return true;
}

bool safeEEPROMRead(int address, uint16_t* value) {
    if (!isAddressSafe(address) || !isAddressSafe(address + 1)) {
        *value = 0xFFFF;
        return false;
    }
    byte lowByte = EEPROM.read(address);
    byte highByte = EEPROM.read(address + 1);
    *value = (highByte << 8) | lowByte;
    return true;
}

// *** State slot address safety (v1.2.4) ***
// Separate guard for state slots (118-129) so scale safety is unaffected.
bool isStateAddressSafe(int address) {
    if (address < STATE_SLOT_1_ADDR || address > STATE_SLOT_END) return false;
    if (address >= PROTECTED_FLAG_START && address <= PROTECTED_FLAG_END) return false;
    return true;
}

// saveStateToSlot() — persists full module state to one EEPROM slot.
// slotAddress: first byte of a 12-byte block (pass STATE_SLOT_1_ADDR for slot 1).
void saveStateToSlot(int slotAddress) {
    // Validate the entire 12-byte range before touching EEPROM
    for (int i = 0; i < 12; i++) {
        if (!isStateAddressSafe(slotAddress + i)) {
            #if DEBUG_MODE
                Serial.println(F("!!! STATE SAVE FAILED - UNSAFE ADDR"));
            #endif
            return;
        }
    }
    int addr = slotAddress;
    uint16_t signature  = STATE_SIGNATURE;
    uint16_t reg        = shiftRegister;
    uint8_t  stepLen    = (uint8_t)currentStepLength;
    uint8_t  pad        = (uint8_t)rotationOffset;  // rotation offset (0-15)
    uint8_t  volRange   = (uint8_t)voltageRange;
    // Capture current filtered pot value at save time
    int rawPot = analogRead(potentiometerPin);
    uint16_t potSnap    = (uint16_t)rawPot;
    uint16_t scalePacked = packScale();
    uint8_t  reserved   = 0;

    EEPROM.put(addr,      signature);    // 2 bytes  (addr 0-1)
    EEPROM.put(addr + 2,  reg);          // 2 bytes  (addr 2-3)
    EEPROM.put(addr + 4,  stepLen);      // 1 byte   (addr 4)
    EEPROM.put(addr + 5,  pad);          // 1 byte   (addr 5)
    EEPROM.put(addr + 6,  volRange);     // 1 byte   (addr 6)
    EEPROM.put(addr + 7,  reserved);     // 1 byte   (addr 7)
    EEPROM.put(addr + 8,  potSnap);      // 2 bytes  (addr 8-9)
    EEPROM.put(addr + 10, scalePacked);  // 2 bytes  (addr 10-11)
    // Total: 12 bytes

    #if DEBUG_MODE
        Serial.print(F("STATE SAVE @"));
        Serial.print(slotAddress);
        Serial.print(F(" reg=0x"));
        Serial.print(shiftRegister, HEX);
        Serial.print(F(" len="));
        Serial.print(currentStepLength);
        Serial.print(F(" range="));
        Serial.print(voltageRange);
        Serial.print(F(" pot="));
        Serial.print(potSnap);
        Serial.print(F(" scale=0x"));
        Serial.println(scalePacked, HEX);
    #endif
}

// loadStateFromSlot() — restores full module state from one EEPROM slot.
// Returns true if a valid state was found and loaded, false if slot is empty.
bool loadStateFromSlot(int slotAddress) {
    for (int i = 0; i < 12; i++) {
        if (!isStateAddressSafe(slotAddress + i)) {
            #if DEBUG_MODE
                Serial.println(F("!!! STATE LOAD FAILED - UNSAFE ADDR"));
            #endif
            return false;
        }
    }
    int addr = slotAddress;
    uint16_t signature;
    EEPROM.get(addr, signature);
    if (signature != STATE_SIGNATURE) {
        #if DEBUG_MODE
            Serial.print(F("STATE LOAD @"));
            Serial.print(slotAddress);
            Serial.println(F(" - EMPTY"));
        #endif
        return false;
    }
    uint16_t reg;
    uint8_t  stepLen, pad, volRange, reserved;
    uint16_t potSnap, scalePacked;

    EEPROM.get(addr + 2,  reg);
    EEPROM.get(addr + 4,  stepLen);
    EEPROM.get(addr + 5,  pad);
    EEPROM.get(addr + 6,  volRange);
    EEPROM.get(addr + 7,  reserved);
    EEPROM.get(addr + 8,  potSnap);
    EEPROM.get(addr + 10, scalePacked);

    // Validate loaded values before applying
    bool validStepLen = (stepLen == 3 || stepLen == 4 || stepLen == 5 ||
                         stepLen == 6 || stepLen == 8 || stepLen == 12 || stepLen == 16);
    bool validRange   = (volRange >= 1 && volRange <= 4);

    if (!validStepLen || !validRange) {
        #if DEBUG_MODE
            Serial.println(F("!!! STATE LOAD - CORRUPT DATA"));
        #endif
        return false;
    }

    // Apply state
    shiftRegister      = reg;
    currentStepLength  = stepLen;
    voltageRange       = volRange;
    stepCounter        = 0;       // always start from step 0 on load
    unpackScale(scalePacked);     // also calls rebuildNoteArray()

    // Reset lock/double state — let the pot re-establish on next updateProbability()
    patternLocked = false;
    patternDouble = false;
    noteArrayValid = false;

    // Restore rotation offset (saved in pad byte, valid range 0-15)
    rotationOffset = (pad < 16) ? (int)pad : 0;

    // Capture baseline for pot pickup
    savedPotValue      = (int)potSnap;
    potBaselineOnLoad  = analogRead(potentiometerPin);  // physical position right now
    potPickupRequired  = true;

    #if DEBUG_MODE
        Serial.print(F("STATE LOAD @"));
        Serial.print(slotAddress);
        Serial.print(F(" reg=0x"));
        Serial.print(shiftRegister, HEX);
        Serial.print(F(" len="));
        Serial.print(currentStepLength);
        Serial.print(F(" range="));
        Serial.print(voltageRange);
        Serial.print(F(" savedPot="));
        Serial.print(savedPotValue);
        Serial.print(F(" physPot="));
        Serial.print(potBaselineOnLoad);
        Serial.print(F(" scale=0x"));
        Serial.println(scalePacked, HEX);
    #endif
    return true;
}

bool isQuantizationActive() {
    return (scaleNoteCount > 0);
}

void addNoteToScale(int noteIndex) {
    int chromaticNote = noteIndex % 12;
    if (!scaleNotes[chromaticNote]) {
        scaleNotes[chromaticNote] = true;
        scaleNoteCount++;
        rebuildNoteArray();
        #if DEBUG_MODE
            Serial.print(F("+ Note "));
            Serial.print(chromaticNote);
            Serial.print(F(" (Count: "));
            Serial.print(scaleNoteCount);
            Serial.println(F(")"));
        #endif
    }
}

void clearScale() {
    for (int i = 0; i < 12; i++) {
        scaleNotes[i] = false;
    }
    scaleNoteCount = 0;
    rebuildNoteArray();
    #if DEBUG_MODE
        Serial.println(F("Scale cleared"));
    #endif
}

int findNearestScaleNote(int noteInOctave) {
    if (scaleNoteCount == 0) return noteInOctave;
    for (int offset = 0; offset < 12; offset++) {
        int upNote = (noteInOctave + offset) % 12;
        if (scaleNotes[upNote]) return upNote;
        int downNote = (noteInOctave - offset + 12) % 12;
        if (scaleNotes[downNote]) return downNote;
    }
    return noteInOctave;
}

float quantizeVoltage(byte cvBits) {
    if (!isQuantizationActive()) {
        float normalizedValue = cvBits / 255.0;
        float voltage = normalizedValue * voltageRange;
        if (voltage > 4.0) voltage = 4.0;
        return voltage;
    }
    int totalSemitones = voltageRange * 12;
    int rawSemitone = (cvBits * totalSemitones) / 255;
    int octave = rawSemitone / 12;
    int noteInOctave = rawSemitone % 12;
    int quantizedNote = findNearestScaleNote(noteInOctave);
    int finalSemitone = octave * 12 + quantizedNote;
    if (finalSemitone > 48) finalSemitone = 48;
    if (finalSemitone < 0) finalSemitone = 0;
    return calibrationValues[finalSemitone];
}

uint16_t packScale() {
    uint16_t packed = 0;
    for (int i = 0; i < 12; i++) {
        if (scaleNotes[i]) packed |= (1 << i);
    }
    return packed;
}

void unpackScale(uint16_t packed) {
    scaleNoteCount = 0;
    for (int i = 0; i < 12; i++) {
        scaleNotes[i] = (packed >> i) & 1;
        if (scaleNotes[i]) scaleNoteCount++;
    }
    rebuildNoteArray();
}

int getScaleSlotAddress(int noteIndex) {
    switch (noteIndex) {
        case 2:  return SCALE_SLOT_D_ADDR;
        case 4:  return SCALE_SLOT_E_ADDR;
        case 5:  return SCALE_SLOT_F_ADDR;
        case 7:  return SCALE_SLOT_G_ADDR;
        case 9:  return SCALE_SLOT_A_ADDR;
        case 11: return SCALE_SLOT_B_ADDR;
        default: return -1;
    }
}

void saveScaleToSlot(int noteIndex) {
    int address = getScaleSlotAddress(noteIndex);
    if (address < 0) return;
    uint16_t packed = packScale();
    if (safeEEPROMWrite(address, packed)) {
        #if DEBUG_MODE
            Serial.print(F("SAVE slot "));
            Serial.print(noteIndex);
            Serial.print(F(" @"));
            Serial.print(address);
            Serial.print(F(" = 0x"));
            Serial.println(packed, HEX);
        #endif
    } else {
        #if DEBUG_MODE
            Serial.println(F("!!! SAVE FAILED - UNSAFE"));
        #endif
    }
}

void loadScaleFromSlot(int noteIndex) {
    int address = getScaleSlotAddress(noteIndex);
    if (address < 0) return;
    uint16_t packed;
    if (!safeEEPROMRead(address, &packed)) {
        #if DEBUG_MODE
            Serial.println(F("!!! LOAD FAILED - UNSAFE"));
        #endif
        return;
    }
    uint16_t signature;
    if (!safeEEPROMRead(SCALE_SIGNATURE_ADDR, &signature) || signature != SCALE_SIGNATURE) {
        #if DEBUG_MODE
            Serial.println(F("!!! NO SIGNATURE - INIT FIRST"));
        #endif
        return;
    }
    if (packed != 0x0000 && packed != 0xFFFF) {
        unpackScale(packed);
        #if DEBUG_MODE
            Serial.print(F("LOAD slot "));
            Serial.print(noteIndex);
            Serial.print(F(" @"));
            Serial.print(address);
            Serial.print(F(" = 0x"));
            Serial.print(packed, HEX);
            Serial.print(F(" ("));
            Serial.print(scaleNoteCount);
            Serial.println(F(" notes)"));
        #endif
    } else {
        #if DEBUG_MODE
            Serial.print(F("LOAD slot "));
            Serial.print(noteIndex);
            Serial.println(F(" - EMPTY"));
        #endif
    }
}

// *** CV Keyboard Mode Functions ***

void enterCVKeyboardMode() {
    inCVKeyboardMode = true;
    pinMode(triggerPin, OUTPUT);
    digitalWrite(triggerPin, LOW);
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            lastKeyboardDebounceTime[r][c] = 0;
            lastKeyboardButtonState[r][c] = false;
        }
    }
    lastHighCDebounceTime = 0;
    lastHighCState = false;
    anyKeyPressed = false;
    lastAnyKeyPressed = false;
    #if DEBUG_MODE
        Serial.println(F(">>> CV KEYBOARD MODE <<<"));
    #endif
}

void handleCVKeyboardOperation(unsigned long currentMillis) {
    anyKeyPressed = false;
    bool octaveUpState = !digitalRead(octaveUpPin);
    bool octaveDownState = !digitalRead(octaveDownPin);
    if (octaveUpState && !lastOctaveUpState && (currentMillis - lastOctaveDebounceTime > debounceDelayButtons)) {
        if (voltageRange < 4) voltageRange++;
        lastOctaveDebounceTime = currentMillis;
    }
    if (octaveDownState && !lastOctaveDownState && (currentMillis - lastOctaveDebounceTime > debounceDelayButtons)) {
        if (voltageRange > 1) voltageRange--;
        lastOctaveDebounceTime = currentMillis;
    }
    lastOctaveUpState = octaveUpState;
    lastOctaveDownState = octaveDownState;
    for (int c = 0; c < cols; c++) {
        digitalWrite(colPins[c], LOW);
        for (int r = 0; r < rows; r++) {
            bool currentState = !digitalRead(rowPins[r]);
            if (currentState != lastKeyboardButtonState[r][c]) {
                if (currentMillis - lastKeyboardDebounceTime[r][c] > keyboardDebounceDelay) {
                    if (currentState) {
                        int noteIndex = r * cols + c + (voltageRange - 1) * 12;
                        if (noteIndex > 48) noteIndex = 48;
                        keyboardTargetVoltage = calibrationValues[noteIndex];
                        if (portamentoRate == 0) {
                            keyboardCurrentVoltage = keyboardTargetVoltage;
                            dac.setVoltage((int)(keyboardCurrentVoltage * (4095 / 4.93)), false);
                        }
                    }
                    lastKeyboardButtonState[r][c] = currentState;
                    lastKeyboardDebounceTime[r][c] = currentMillis;
                }
            }
            if (lastKeyboardButtonState[r][c]) anyKeyPressed = true;
        }
        digitalWrite(colPins[c], HIGH);
    }
    bool currentHighCState = !digitalRead(highCButton);
    if (currentHighCState != lastHighCState) {
        if (currentMillis - lastHighCDebounceTime > keyboardDebounceDelay) {
            if (currentHighCState) {
                int noteIndex = 48;
                switch (voltageRange) {
                    case 1: noteIndex = 12; break;
                    case 2: noteIndex = 24; break;
                    case 3: noteIndex = 36; break;
                    case 4: noteIndex = 48; break;
                }
                keyboardTargetVoltage = calibrationValues[noteIndex];
                if (portamentoRate == 0) {
                    keyboardCurrentVoltage = keyboardTargetVoltage;
                    dac.setVoltage((int)(keyboardCurrentVoltage * (4095 / 4.93)), false);
                }
            }
            lastHighCState = currentHighCState;
            lastHighCDebounceTime = currentMillis;
        }
    }
    if (lastHighCState) anyKeyPressed = true;
    if (anyKeyPressed != lastAnyKeyPressed) {
        digitalWrite(triggerPin, anyKeyPressed ? HIGH : LOW);
        lastAnyKeyPressed = anyKeyPressed;
    }
    handlePotentiometerKeyboardMode();
    updatePortamento();
}

void handlePotentiometerKeyboardMode() {
    int potValue = analogRead(potentiometerPin);
    int effectiveRange = constrain(potValue, 0, 800);
    if (effectiveRange < 16) {
        portamentoRate = 0;
    } else if (effectiveRange > 1020) {
        portamentoRate = 0.1;
    } else {
        portamentoRate = map(effectiveRange, 1020, 16, 0, 100) / 1023.0;
        portamentoRate = constrain(portamentoRate, 0, 0.1);
    }
}

void updatePortamento() {
    if (portamentoRate > 0 && keyboardCurrentVoltage != keyboardTargetVoltage) {
        unsigned long currentTime = micros();
        if (currentTime - lastPortamentoUpdate >= 500) {
            float difference = keyboardTargetVoltage - keyboardCurrentVoltage;
            if (abs(difference) > 0.001) {
                keyboardCurrentVoltage += difference * portamentoRate;
                dac.setVoltage((int)(keyboardCurrentVoltage * (4095 / 4.93)), false);
            } else {
                keyboardCurrentVoltage = keyboardTargetVoltage;
                dac.setVoltage((int)(keyboardCurrentVoltage * (4095 / 4.93)), false);
            }
            lastPortamentoUpdate = currentTime;
        }
    }
}

float bitsToVoltage(byte bits) {
    return quantizeVoltage(bits);
}

// rebuildNoteArray() — pre-computes DAC values for every step in the current
// locked or double-locked pattern. Must be called whenever the shift register
// pattern, scale, step length, voltage range, or lock state changes.
// Resets rotationOffset to 0 since the underlying pattern has changed.
void rebuildNoteArray() {
    if (!patternLocked && !patternDouble) {
        noteArrayValid = false;
        return;
    }
    unsigned int src = patternLocked ? lockedPattern : doublePattern;
    int effectiveLength = patternDouble ? currentStepLength * 2 : currentStepLength;
    if (effectiveLength > 16) effectiveLength = 16;

    // Simulate stepping through the pattern to extract output bits per step,
    // mirroring the exact same bit extraction logic used in advanceTuringMachine().
    unsigned int reg = src;
    for (int i = 0; i < effectiveLength; i++) {
        unsigned int pattern = reg & ((1 << currentStepLength) - 1);
        byte outputBits;
        if (currentStepLength >= 8) {
            outputBits = (pattern >> (currentStepLength - 8)) & 0xFF;
        } else {
            switch (currentStepLength) {
                case 3: { byte p = pattern & 0x07; outputBits = (p << 5) | (p << 2) | (p >> 1); break; }
                case 4: { byte p = pattern & 0x0F; outputBits = (p << 4) | p; break; }
                case 5: { byte p = pattern & 0x1F; outputBits = (p << 3) | (p >> 2); break; }
                case 6: { byte p = pattern & 0x3F; outputBits = (p << 2) | (p >> 4); break; }
                default: outputBits = pattern & 0xFF; break;
            }
        }
        float voltage = bitsToVoltage(outputBits);
        noteArray[i] = (uint16_t)(voltage * (4095.0 / 4.93));
        // Advance the register one step (feedback bit = MSB, no flip = locked)
        bool feedbackBit = (reg >> (currentStepLength - 1)) & 1;
        if (patternDouble) feedbackBit = !feedbackBit;  // double mode inverts
        reg = (reg << 1) | feedbackBit;
    }
    rotationOffset = 0;
    noteArrayValid = true;
    #if DEBUG_MODE
        Serial.print(F("NOTE ARRAY rebuilt len="));
        Serial.print(effectiveLength);
        Serial.print(F(" steps: "));
        for (int i = 0; i < effectiveLength; i++) {
            Serial.print(noteArray[i]);
            Serial.print(' ');
        }
        Serial.println();
    #endif
}

// applyRotation() — immediately shifts the rotation offset by one step.
// direction: +1 = forward, -1 = backward.
// Only active when a stable pattern exists (locked or double).
void applyRotation(int direction) {
    if (!noteArrayValid) return;
    if (!patternLocked && !patternDouble) return;
    int effectiveLength = patternDouble ? currentStepLength * 2 : currentStepLength;
    if (effectiveLength > 16) effectiveLength = 16;
    rotationOffset = (rotationOffset + direction + effectiveLength) % effectiveLength;
    #if DEBUG_MODE
        Serial.print(F("ROTATE offset="));
        Serial.println(rotationOffset);
    #endif
}

void updateProbability() {
    int potValue = analogRead(potentiometerPin);
    static int lastPotValue = 512;
    potValue = (lastPotValue * 3 + potValue) / 4;
    lastPotValue = potValue;

    // *** Pot pickup after state load (v1.2.4) ***
    // After loadStateFromSlot(), ignore the physical pot until it has moved
    // more than 30 ADC counts from its position at load time. Once it moves,
    // release pickup and return to normal live control.
    if (potPickupRequired) {
        if (abs(potValue - potBaselineOnLoad) > 30) {
            potPickupRequired = false;
            // Fall through and use live potValue immediately
        } else {
            // Substitute the saved pot value so probability stays where it was
            potValue = savedPotValue;
        }
    }
    PotMode oldMode = currentPotMode;
    if (potValue <= POT_DOUBLE_MAX) {
        currentPotMode = POT_MODE_DOUBLE;
    } else if (potValue >= POT_LOCKED_MIN) {
        currentPotMode = POT_MODE_LOCKED;
    } else if (potValue >= POT_RANDOM_MIN && potValue <= POT_RANDOM_MAX) {
        currentPotMode = POT_MODE_RANDOM;
    } else if (potValue < POT_RANDOM_MIN) {
        currentPotMode = POT_MODE_VARIABLE_LOW;
    } else {
        currentPotMode = POT_MODE_VARIABLE_HIGH;
    }
    switch (currentPotMode) {
        case POT_MODE_DOUBLE:
            probabilityValue = 0.0;
            doubleLengthMode = true;
            if (!patternDouble) {
                doublePattern = shiftRegister;
                patternDouble = true;
                rebuildNoteArray();
            }
            break;
        case POT_MODE_VARIABLE_LOW:
            if (potValue < 420) {
                probabilityValue = map(potValue, 60, 420, 10, 150) / 1000.0;
            } else {
                float normalized = (potValue - 420.0) / (500.0 - 420.0);
                float exponential = normalized * normalized;
                probabilityValue = 0.15 + (exponential * 0.85);
            }
            doubleLengthMode = false;
            break;
        case POT_MODE_RANDOM:
            probabilityValue = 1.0;
            doubleLengthMode = false;
            break;
        case POT_MODE_VARIABLE_HIGH:
            if (potValue > 600) {
                probabilityValue = map(potValue, 600, 950, 150, 10) / 1000.0;
            } else {
                float normalized = (potValue - 510.0) / (600.0 - 510.0);
                float exponential = (1.0 - normalized) * (1.0 - normalized);
                probabilityValue = 0.15 + (exponential * 0.85);
            }
            doubleLengthMode = false;
            break;
        case POT_MODE_LOCKED:
            probabilityValue = 0.0;
            doubleLengthMode = false;
            if (!patternLocked) {
                lockedPattern = shiftRegister;
                patternLocked = true;
                rebuildNoteArray();
            }
            break;
    }
    if (oldMode == POT_MODE_LOCKED && currentPotMode != POT_MODE_LOCKED) { patternLocked = false; noteArrayValid = false; }
    if (oldMode == POT_MODE_DOUBLE && currentPotMode != POT_MODE_DOUBLE) { patternDouble = false; noteArrayValid = false; }
}

void advanceTuringMachine() {
    if (resetPending && (patternLocked || patternDouble)) {
        if (patternLocked) {
            shiftRegister = lockedPattern;
        } else if (patternDouble) {
            shiftRegister = doublePattern;
        }
        stepCounter = 1;  // advance past reset note so next clock plays step 2,
                          // not a double-hit on step 1
        resetPending = false;
        // Output the rotated step 0 — rotation offset is preserved so reset
        // lands on the user's chosen downbeat, not the raw pattern start.
        if (noteArrayValid) {
            dac.setVoltage(noteArray[rotationOffset % 16], false);
        } else {
            unsigned int pattern = shiftRegister & ((1 << currentStepLength) - 1);
            byte outputBits;
            if (currentStepLength >= 8) {
                outputBits = (pattern >> (currentStepLength - 8)) & 0xFF;
            } else {
                switch (currentStepLength) {
                    case 3: { byte p = pattern & 0x07; outputBits = (p << 5) | (p << 2) | (p >> 1); break; }
                    case 4: { byte p = pattern & 0x0F; outputBits = (p << 4) | p; break; }
                    case 5: { byte p = pattern & 0x1F; outputBits = (p << 3) | (p >> 2); break; }
                    case 6: { byte p = pattern & 0x3F; outputBits = (p << 2) | (p >> 4); break; }
                    default: outputBits = pattern & 0xFF; break;
                }
            }
            float voltage = bitsToVoltage(outputBits);
            dac.setVoltage((int)(voltage * (4095 / 4.93)), false);
        }
        return;
    }

    // *** Locked / Double mode: output from pre-computed note array ***
    if ((patternLocked || patternDouble) && noteArrayValid) {
        int effectiveLength = patternDouble ? currentStepLength * 2 : currentStepLength;
        if (effectiveLength > 16) effectiveLength = 16;
        int outputIndex = (stepCounter + rotationOffset) % effectiveLength;

        // Set/clear bit still edits the underlying shift register and
        // triggers a note array rebuild so the change is heard immediately.
        if (clearBitButtonHeld) {
            int bitPosition = currentStepLength - 1 - stepCounter;
            shiftRegister &= ~(1 << bitPosition);
            if (patternLocked) lockedPattern = shiftRegister;
            else if (patternDouble) doublePattern = shiftRegister;
            rebuildNoteArray();
            rotationOffset = 0;  // reset after edit so user hears change from step 0
        }
        if (setBitButtonHeld) {
            int bitPosition = currentStepLength - 1 - stepCounter;
            shiftRegister |= (1 << bitPosition);
            if (patternLocked) lockedPattern = shiftRegister;
            else if (patternDouble) doublePattern = shiftRegister;
            rebuildNoteArray();
            rotationOffset = 0;
        }

        dac.setVoltage(noteArray[outputIndex], false);

        #if DEBUG_MODE
            Serial.print(F("S:")); Serial.print(stepCounter);
            Serial.print(F(" R:")); Serial.print(rotationOffset);
            Serial.print(F(" I:")); Serial.print(outputIndex);
            Serial.print(F(" DAC:")); Serial.print(noteArray[outputIndex]);
            Serial.println(F(" [LOCKED]"));
        #endif

        stepCounter++;
        if (stepCounter >= effectiveLength) stepCounter = 0;
        return;
    }

    // *** Random / Variable mode: standard bit-based output ***
    unsigned int pattern = shiftRegister & ((1 << currentStepLength) - 1);
    byte outputBits;
    if (currentStepLength >= 8) {
        outputBits = (pattern >> (currentStepLength - 8)) & 0xFF;
    } else {
        switch (currentStepLength) {
            case 3: { byte p = pattern & 0x07; outputBits = (p << 5) | (p << 2) | (p >> 1); break; }
            case 4: { byte p = pattern & 0x0F; outputBits = (p << 4) | p; break; }
            case 5: { byte p = pattern & 0x1F; outputBits = (p << 3) | (p >> 2); break; }
            case 6: { byte p = pattern & 0x3F; outputBits = (p << 2) | (p >> 4); break; }
            default: outputBits = pattern & 0xFF; break;
        }
    }
    if (clearBitButtonHeld) {
        int bitPosition = currentStepLength - 1 - stepCounter;
        shiftRegister &= ~(1 << bitPosition);
        if (patternLocked) lockedPattern = shiftRegister;
        else if (patternDouble) doublePattern = shiftRegister;
    }
    if (setBitButtonHeld) {
        int bitPosition = currentStepLength - 1 - stepCounter;
        shiftRegister |= (1 << bitPosition);
        if (patternLocked) lockedPattern = shiftRegister;
        else if (patternDouble) doublePattern = shiftRegister;
    }
    float voltage = bitsToVoltage(outputBits);
    dac.setVoltage((int)(voltage * (4095 / 4.93)), false);
    #if DEBUG_MODE
        Serial.print(F("S:")); Serial.print(stepCounter);
        Serial.print(F(" B:")); Serial.print(outputBits);
        Serial.print(F(" V:")); Serial.print(voltage);
        if (isQuantizationActive()) Serial.print(F(" [Q]"));
        Serial.println();
    #endif
    int bitPosition = currentStepLength - 1;
    bool feedbackBit = (shiftRegister >> bitPosition) & 1;
    bool shouldFlip = false;
    if (doubleLengthMode) {
        shouldFlip = true;
    } else if (probabilityValue > 0.0) {
        int randomValue = random(1000);
        int threshold = (int)(probabilityValue * 1000);
        shouldFlip = (randomValue < threshold);
    }
    if (shouldFlip) feedbackBit = !feedbackBit;
    shiftRegister = (shiftRegister << 1) | feedbackBit;
    stepCounter++;
    if (doubleLengthMode) {
        if (stepCounter >= currentStepLength * 2) stepCounter = 0;
    } else {
        if (stepCounter >= currentStepLength) stepCounter = 0;
    }
}

void handleButtonMatrix() {
    bool isAnyButtonPressed = false;
    bool shiftButtonState = !digitalRead(highCButton);
    if (shiftButtonState && !lastShiftButtonState) {
        shiftButtonPressTime = millis();
    } else if (shiftButtonState && !shiftModeActive &&
               (millis() - shiftButtonPressTime >= shiftModeActivationTime)) {
        shiftModeActive = true;
    } else if (!shiftButtonState && lastShiftButtonState) {
        shiftModeActive = false;
    }
    lastShiftButtonState = shiftButtonState;
    bool dSharpButtonState = false;
    bool fSharpButtonState = false;
    bool cButtonPressed = false;
    for (int c = 0; c < cols; c++) {
        digitalWrite(colPins[c], LOW);
        for (int r = 0; r < rows; r++) {
            bool currentButtonState = !digitalRead(rowPins[r]);
            int noteIndex = r * cols + c;
            if (noteIndex == 0 && currentButtonState) cButtonPressed = true;
            if (noteIndex == 3 && currentButtonState && shiftModeActive) dSharpButtonState = true;
            if (noteIndex == 6 && currentButtonState && shiftModeActive) fSharpButtonState = true;
            if (currentButtonState != turingLastButtonState[r][c]) {
                if (millis() - lastDebounceTime[r][c] > debounceDelayButtons) {
                    turingLastButtonState[r][c] = currentButtonState;
                    lastDebounceTime[r][c] = millis();
                    if (currentButtonState) {
                        isAnyButtonPressed = true;
                        if (shiftModeActive) {
                            switch (noteIndex) {
                                case 0:  currentStepLength = 3; break;
                                case 1:  resetPending = true; break;
                                case 2:  currentStepLength = 4; break;
                                case 4:  currentStepLength = 5; break;
                                case 5:  currentStepLength = 6; break;
                                case 7:  currentStepLength = 8; break;
                                case 8:  applyRotation(-1); break;
                                case 9:  currentStepLength = 12; break;
                                case 10: applyRotation(+1); break;
                                case 11: currentStepLength = 16; break;
                            }
                        } else if (octaveDownLongHoldActive) {
                            // Save: white key scale slots OR black keys = state slots
                            if (noteIndex == 1) {
                                saveStateToSlot(STATE_SLOT_1_ADDR);
                                modeChangeOccurred = true;
                            } else if (noteIndex == 3) {
                                saveStateToSlot(STATE_SLOT_2_ADDR);
                                modeChangeOccurred = true;
                            } else if (noteIndex == 6) {
                                saveStateToSlot(STATE_SLOT_3_ADDR);
                                modeChangeOccurred = true;
                            } else if (noteIndex == 8) {
                                saveStateToSlot(STATE_SLOT_4_ADDR);
                                modeChangeOccurred = true;
                            } else if (noteIndex == 10) {
                                saveStateToSlot(STATE_SLOT_5_ADDR);
                                modeChangeOccurred = true;
                            } else if (noteIndex == 2 || noteIndex == 4 || noteIndex == 5 ||
                                noteIndex == 7 || noteIndex == 9 || noteIndex == 11) {
                                saveScaleToSlot(noteIndex);
                                modeChangeOccurred = true;
                            }
                        } else if (octaveUpLongHoldActive) {
                            // Load: white key scale slots OR black keys = state slots
                            if (noteIndex == 1) {
                                loadStateFromSlot(STATE_SLOT_1_ADDR);
                                modeChangeOccurred = true;
                            } else if (noteIndex == 3) {
                                loadStateFromSlot(STATE_SLOT_2_ADDR);
                                modeChangeOccurred = true;
                            } else if (noteIndex == 6) {
                                loadStateFromSlot(STATE_SLOT_3_ADDR);
                                modeChangeOccurred = true;
                            } else if (noteIndex == 8) {
                                loadStateFromSlot(STATE_SLOT_4_ADDR);
                                modeChangeOccurred = true;
                            } else if (noteIndex == 10) {
                                loadStateFromSlot(STATE_SLOT_5_ADDR);
                                modeChangeOccurred = true;
                            } else if (noteIndex == 2 || noteIndex == 4 || noteIndex == 5 ||
                                noteIndex == 7 || noteIndex == 9 || noteIndex == 11) {
                                loadScaleFromSlot(noteIndex);
                                modeChangeOccurred = true;
                            }
                        } else {
                            addNoteToScale(noteIndex);
                        }
                    }
                }
            } else if (currentButtonState) {
                isAnyButtonPressed = true;
            }
        }
        digitalWrite(colPins[c], HIGH);
    }
    if (octaveUpLongHoldActive && cButtonPressed) {
        clearScale();
        modeChangeOccurred = true;
    }
    clearBitButtonHeld = dSharpButtonState;
    setBitButtonHeld = fSharpButtonState;
    digitalWrite(LED_BUILTIN, isAnyButtonPressed || shiftModeActive);
}

void handleOctaveButtons() {
    unsigned long currentMillis = millis();
    bool octaveUpState = !digitalRead(octaveUpPin);
    bool octaveDownState = !digitalRead(octaveDownPin);
    if (octaveUpState && octaveUpPressTime == 0) {
        octaveUpPressTime = currentMillis;
        modeChangeOccurred = false;
    } else if (octaveUpState && !octaveUpLongHoldActive &&
               (currentMillis - octaveUpPressTime >= shiftModeActivationTime)) {
        octaveUpLongHoldActive = true;
    } else if (!octaveUpState) {
        if (octaveUpLongHoldActive) {
            octaveUpLongHoldActive = false;
        } else if (currentMillis - octaveUpPressTime < shiftModeActivationTime && !modeChangeOccurred) {
            if (voltageRange < 4) { voltageRange++; rebuildNoteArray(); }
        }
        octaveUpPressTime = 0;
    }
    if (octaveDownState && octaveDownPressTime == 0) {
        octaveDownPressTime = currentMillis;
        modeChangeOccurred = false;
    } else if (octaveDownState && !octaveDownLongHoldActive &&
               (currentMillis - octaveDownPressTime >= shiftModeActivationTime)) {
        octaveDownLongHoldActive = true;
    } else if (!octaveDownState) {
        if (octaveDownLongHoldActive) {
            octaveDownLongHoldActive = false;
        } else if (currentMillis - octaveDownPressTime < shiftModeActivationTime &&
                   !modeChangeOccurred && !octaveUpLongHoldActive) {
            if (voltageRange > 1) { voltageRange--; rebuildNoteArray(); }
        }
        octaveDownPressTime = 0;
    }
    if ((octaveUpState != lastOctaveUpState || octaveDownState != lastOctaveDownState) &&
        (millis() - lastOctaveDebounceTime > debounceDelayButtons)) {
        lastOctaveUpState = octaveUpState;
        lastOctaveDownState = octaveDownState;
        lastOctaveDebounceTime = millis();
    }
}

void handleClock() {
    bool currentClockState = digitalRead(triggerPin);
    if (currentClockState == HIGH && lastClockState == LOW) {
        advanceTuringMachine();
    }
    lastClockState = currentClockState;
}

void setup() {
    Wire.begin();
    dac.begin(0x60);
    #if DEBUG_MODE
        Serial.begin(9600);
        Serial.println(F("Shroud of Turing v1.2.0"));
    #endif
    for (int r = 0; r < rows; r++) {
        pinMode(rowPins[r], INPUT_PULLUP);
        for (int c = 0; c < cols; c++) {
            pinMode(colPins[c], OUTPUT);
            digitalWrite(colPins[c], HIGH);
            turingLastButtonState[r][c] = false;
            lastDebounceTime[r][c] = 0;
        }
    }
    pinMode(octaveUpPin, INPUT_PULLUP);
    pinMode(octaveDownPin, INPUT_PULLUP);
    pinMode(triggerPin, INPUT);
    pinMode(highCButton, INPUT_PULLUP);
    pinMode(potentiometerPin, INPUT);
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(triggerPin, LOW);

    // Boot mode detection (2-second window)
    // Priority: Calibration > CV Keyboard > Normal Turing Machine
    unsigned long bootTime = millis();
    while (millis() - bootTime < 2000) {
        bool octaveUpPressed = !digitalRead(octaveUpPin);
        bool octaveDownPressed = !digitalRead(octaveDownPin);
        bool shiftPressed = !digitalRead(highCButton);
        if (octaveUpPressed && octaveDownPressed) {
            inCalibrationMode = true;
            digitalWrite(triggerPin, HIGH);
            #if DEBUG_MODE
                Serial.println(F(">>> CALIBRATION MODE <<<"));
            #endif
            break;
        } else if (shiftPressed && !octaveUpPressed && !octaveDownPressed) {
            enterCVKeyboardMode();
            break;
        }
        delay(50);
    }
    #if DEBUG_MODE
        if (!inCalibrationMode && !inCVKeyboardMode) Serial.println(F(">>> TURING MACHINE MODE <<<"));
    #endif

    handleEEPROM();

    // Seed random number generator from noise sources
    unsigned long seed = 0;
    for (int i = 0; i < 8; i++) {
        seed ^= analogRead(A1) << i;
        seed ^= analogRead(A2) << (i+8);
        seed ^= micros();
        delay(1);
    }
    for (int i = 0; i < 8; i++) {
        seed ^= EEPROM.read(i) << (i * 2);
    }
    randomSeed(seed);

    shiftRegister = random(65536);
    if (shiftRegister == 0) shiftRegister = 0x5A5A;

    clearScale();

    // Safe EEPROM initialization with validation
    uint16_t signature;
    if (safeEEPROMRead(SCALE_SIGNATURE_ADDR, &signature)) {
        if (signature != SCALE_SIGNATURE) {
            if (safeEEPROMWrite(SCALE_SIGNATURE_ADDR, SCALE_SIGNATURE)) {
                for (int addr = SCALE_SLOT_D_ADDR; addr <= SCALE_SLOT_B_ADDR; addr += 2) {
                    safeEEPROMWrite(addr, (uint16_t)0x0000);
                }
            }
        }
    }
}

void loop() {
    unsigned long currentMillis = millis();
    if (inCalibrationMode) {
        handleCalibrationMode();
    } else if (inCVKeyboardMode) {
        handleCVKeyboardOperation(currentMillis);
    } else {
        updateProbability();
        handleButtonMatrix();
        handleOctaveButtons();
        handleClock();
    }
}
