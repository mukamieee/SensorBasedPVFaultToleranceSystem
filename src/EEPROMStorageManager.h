
#pragma once

#include <Arduino.h>
#include <EEPROM.h>

class EEPROMStorageManager {

  private:

    // Identifies whether valid settings have been saved before
    const uint32_t EEPROM_MAGIC = 0x534F4C52;

    // Structure containing settings that must survive a restart
    struct SavedSettings {
      uint32_t magicNumber;
      char pinHash[11];
      float maxTemperature;
      float maxForwardCurrent;
      bool pinSaved;
      bool configSaved;
    };

    // Holds the settings while the Arduino is running
    SavedSettings settings;

    // EEPROM address where the settings structure begins
    const int SETTINGS_ADDRESS = 0;

    // Beginning of the event log area
    const int EVENTS_START = 32;

    // Identifies a valid event record
    const uint16_t EVENT_SIGNATURE = 0xA55A;

    // Structure representing one saved event
    struct EventRecord {
      uint16_t signature;
      uint16_t sequence;
      uint32_t uptimeMs;
      uint16_t faultId;
      uint8_t eventCode;
      uint8_t stateCode;
    };

  public:

    EEPROMStorageManager() {
      // No EEPROM reading or writing happens here
    }

    // Checks whether previously saved settings are valid
    void init() {

      EEPROM.get(SETTINGS_ADDRESS, settings);

      if (settings.magicNumber != EEPROM_MAGIC) {

        // First use: establish default settings
        settings.magicNumber = EEPROM_MAGIC;
        settings.pinHash[0] = '\0';
        settings.maxTemperature = 65.0;
        settings.maxForwardCurrent = 0.0;
        settings.pinSaved = false;
        settings.configSaved = false;

        EEPROM.put(SETTINGS_ADDRESS, settings);
      }
    }

    // Checks whether an owner PIN hash exists
    bool hasSavedPIN() {
      return settings.pinSaved;
    }

    // Retrieves the saved PIN hash
    String readPINHash() {
      return String(settings.pinHash);
    }

    // Saves a new PIN hash
    void savePINHash(String hashStr) {

      hashStr.toCharArray(settings.pinHash, sizeof(settings.pinHash));

      settings.pinSaved = true;

      EEPROM.put(SETTINGS_ADDRESS, settings);

      Serial.println("[SECURITY] PIN hash saved to EEPROM.");
    }

    // Removes the saved PIN hash after an intentional PIN reset
    void clearSavedPIN() {

      settings.pinHash[0] = '\0';
      settings.pinSaved = false;

      EEPROM.put(SETTINGS_ADDRESS, settings);

      Serial.println("[SECURITY] Saved PIN hash cleared.");
    }

    // Checks whether safety configuration exists
    bool hasConfiguration() {
      return settings.configSaved;
    }

    // Saves the safety thresholds
    void saveConfiguration(float maxTemp, float maxCurrent) {

      settings.maxTemperature = maxTemp;
      settings.maxForwardCurrent = maxCurrent;
      settings.configSaved = true;

      EEPROM.put(SETTINGS_ADDRESS, settings);

      Serial.println("[SYSTEM] Safety configuration saved to EEPROM.");
    }

    // Retrieves the saved safety thresholds
    bool loadConfiguration(float &outTemp, float &outCurrent) {

      if (!settings.configSaved) {
        return false;
      }

      outTemp = settings.maxTemperature;
      outCurrent = settings.maxForwardCurrent;

      return true;
    }

    // Counts the number of valid event records
    uint16_t getEventCount() {

      uint16_t count = 0;

      for (int address = EVENTS_START;
           address + sizeof(EventRecord) <= EEPROM.length();
           address += sizeof(EventRecord)) {

        EventRecord record;
        EEPROM.get(address, record);

        // Stop at the first unused or invalid record
        if (record.signature != EVENT_SIGNATURE ||
            record.eventCode < 1 ||
            record.eventCode > 10) {
          break;
        }

        count++;
      }

      return count;
    }

    // Saves one event into EEPROM
    bool saveEvent(uint8_t eventCode,
                   uint16_t faultId,
                   uint8_t stateCode) {

      // Only accept recognised event codes
      if (eventCode < 1 || eventCode > 10) {
        Serial.println("[EEPROM] Invalid event code.");
        return false;
      }

      uint16_t count = getEventCount();

      int address = EVENTS_START + count * sizeof(EventRecord);

      // Check whether enough EEPROM space remains
      if (address + sizeof(EventRecord) > EEPROM.length()) {

        Serial.println("[EEPROM] Event storage full!");
        return false;
      }

      EventRecord record;

      record.signature = EVENT_SIGNATURE;
      record.sequence = count + 1;
      record.uptimeMs = millis();
      record.faultId = faultId;
      record.eventCode = eventCode;
      record.stateCode = stateCode;

      EEPROM.put(address, record);

      Serial.println("[EEPROM] Event saved. Number: " +
                     String(record.sequence));

      return true;
    }

    // Saves owner login and logout events
    void logAccess(String nodeId,
                   bool wasSuccessful,
                   String action) {

      if (action == "MANUAL_LOGOUT") {
        saveEvent(3, 0, 0);
      }
      else if (wasSuccessful) {
        saveEvent(1, 0, 0);
      }
      else {
        saveEvent(2, 0, 0);
      }
    }

    // Saves fault, configuration and maintenance events
    void logMaintenance(String nodeId,
                        unsigned int faultId,
                        String actionTaken,
                        String systemState) {

      uint8_t eventCode = 0;

      if (actionTaken == "FAULT_DETECTED") {
        eventCode = 4;
      }
      else if (actionTaken == "AUTO_RECOVERED") {
        eventCode = 5;
      }
      else if (actionTaken == "HARD_LOCKOUT") {
        eventCode = 6;
      }
      else if (actionTaken == "TECH_RESET_APPLIED") {
        eventCode = 7;
      }
      else if (actionTaken == "PIN_RESET_INITIATED") {
        eventCode = 8;
      }
      else if (actionTaken == "SD_EJECT_MODE") {
        eventCode = 9;
      }
      else if (actionTaken == "CONFIGURATION_CHANGED") {
        eventCode = 10;
      }

      // Do not save unrecognised event types
      if (eventCode == 0) {
        Serial.println("[EEPROM] Unknown event type.");
        return;
      }

      // Determine the relay state at the time of the event
      uint8_t relayState = 2; // Unknown

      if (systemState.indexOf("RELAY_OPEN") >= 0 ||
          systemState.indexOf("ISOLATED") >= 0) {
        relayState = 1;
      }
      else if (systemState.indexOf("RELAY_CLOSED") >= 0) {
        relayState = 0;
      }

      saveEvent(eventCode, (uint16_t)faultId, relayState);
    }
};
