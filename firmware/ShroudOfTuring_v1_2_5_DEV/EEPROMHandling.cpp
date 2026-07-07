#include "EEPROMHandling.h"

void handleEEPROM() {
    // Check for EEPROM signature - initialise with defaults if not found
    int eepromSignature;
    EEPROM.get(EEPROM_SIGNATURE_ADDR, eepromSignature);

    if (eepromSignature != EEPROM_SIGNATURE_VALUE) {
        if (!testMode) {
            EEPROM.put(EEPROM_SIGNATURE_ADDR, EEPROM_SIGNATURE_VALUE);
            for (int i = 0; i < 49; i++) {
                EEPROM.put(EEPROM_CALIBRATION_START_ADDR + i * sizeof(float), defaultCalibrationValues[i]);
            }
            EEPROM.put(EEPROM_INTEGER_FLAG_ADDR, 0);
        }
    }

    // Check integer flag - convert from float format if needed
    int integerFlag;
    EEPROM.get(EEPROM_INTEGER_FLAG_ADDR, integerFlag);

    if (integerFlag != EEPROM_INTEGER_FLAG_VALUE) {
        for (int i = 0; i < 49; i++) {
            EEPROM.get(EEPROM_CALIBRATION_START_ADDR + i * sizeof(float), calibrationValues[i]);
            int scaledValue = (int)(calibrationValues[i] * 1000);
            if (!testMode) {
                EEPROM.put(EEPROM_CALIBRATION_START_ADDR + i * sizeof(int), scaledValue);
            }
        }
        if (!testMode) {
            EEPROM.put(EEPROM_INTEGER_FLAG_ADDR, EEPROM_INTEGER_FLAG_VALUE);
        }
    }

    // Load calibration values from EEPROM
    for (int i = 0; i < 49; i++) {
        int scaledValue;
        EEPROM.get(EEPROM_CALIBRATION_START_ADDR + i * sizeof(int), scaledValue);
        calibrationValues[i] = scaledValue / 1000.0;
    }

    printEEPROMUsage();
}

void printEEPROMUsage() {
    // Serial output disabled in production builds
    // Enable DEBUG_MODE in main firmware for diagnostic output
}
