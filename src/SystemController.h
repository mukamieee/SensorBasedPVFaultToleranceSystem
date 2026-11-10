#pragma once

#include <Arduino.h>

#include "SecurityAuthenticator.h"
#include "SafetyConfiguration.h"
#include "RelayModule.h"
#include "TelemetrySensors.h"
#include "SDFileManager.h"
#include "UserInterface.h"
#include "KeypadManager.h"
#include "FaultManager.h"
#include "EEPROMStorageManager.h"

class SystemController {

  private:

    const String NODE_ID = "NODE_001";

    String currentInput;
    bool isUnlocked;
    bool isPinSetupComplete;

    unsigned long lastDisplayUpdate;
    unsigned long lastLogTime;

    // SD card status
    bool sdAvailable;
    bool sdLoggingEnabled;

    // Menu status
    bool inTechnicianMenu;
    bool inConfigMode;
    bool typingVoltage;

    // Safe-eject confirmation
    bool sdEjectConfirmed;

    String inputWattage;
    String inputVoltage;

    // Hardware and software modules
    SecurityAuthenticator auth;
    SafetyConfiguration safety;
    RelayModule relay;
    TelemetrySensors sensors;
    SDFileManager sdManager;
    UserInterface ui;
    KeypadManager keypad;
    FaultManager faultManager;
    EEPROMStorageManager eepromManager;


    // ------------------------------------------------
    // HELPER: SAVE ACCESS EVENTS
    // ------------------------------------------------
    void recordAccess(bool wasSuccessful, String action) {

      // EEPROM is the primary event backup
      eepromManager.logAccess(
        NODE_ID,
        wasSuccessful,
        action
      );

      // SD is an additional copy when enabled
      if (sdAvailable && sdLoggingEnabled) {

        sdManager.logAccess(
          NODE_ID,
          wasSuccessful,
          action
        );
      }
    }


    // ------------------------------------------------
    // HELPER: SAVE MAINTENANCE EVENTS
    // ------------------------------------------------
    void recordMaintenance(
      unsigned int faultId,
      String actionTaken,
      String systemState
    ) {

      // Always attempt to save the event to EEPROM
      eepromManager.logMaintenance(
        NODE_ID,
        faultId,
        actionTaken,
        systemState
      );

      // Also save to SD if logging is enabled
      if (sdAvailable && sdLoggingEnabled) {

        sdManager.logMaintenance(
          NODE_ID,
          faultId,
          actionTaken,
          systemState
        );
      }
    }


    // ------------------------------------------------
    // HELPER: ENTER CONFIGURATION MODE
    // ------------------------------------------------
    void enterConfigMode() {

      inConfigMode = true;
      inTechnicianMenu = false;

      inputWattage = "";
      inputVoltage = "";

      typingVoltage = false;

      Serial.println("[SYSTEM] Configuration mode opened.");
    }


    // ------------------------------------------------
    // HELPER: SAFELY REMOVE SD CARD
    // ------------------------------------------------
    void performSafeEject() {

      // Do nothing if SD logging is already disabled
      if (!sdLoggingEnabled) {
        return;
      }

      // Record the event before stopping SD logging.
      // This event is saved to EEPROM and, while still
      // enabled, also copied to the SD card.
      recordMaintenance(
        0,
        "SD_EJECT_MODE",
        "SD_LOGGING_STOPPED"
      );

      // Disable future SD card writes
      sdManager.stopLogging();

      sdLoggingEnabled = false;

      sdEjectConfirmed = true;
      inTechnicianMenu = false;

      Serial.println("[SYSTEM] SD logging stopped.");
      Serial.println("[SYSTEM] Software safe-eject mode active.");

      ui.renderSafeEject();
    }


  public:

    // ------------------------------------------------
    // CONSTRUCTOR
    // ------------------------------------------------
    SystemController() :

      auth(),
      safety(65.0, 0.0),
      relay(22),
      sensors(A0, A1, 30),
      sdManager(53)

    {

      currentInput = "";

      isUnlocked = false;
      isPinSetupComplete = false;

      sdAvailable = false;
      sdLoggingEnabled = false;

      inTechnicianMenu = false;
      inConfigMode = false;
      typingVoltage = false;

      sdEjectConfirmed = false;

      inputWattage = "";
      inputVoltage = "";

      lastDisplayUpdate = 0;
      lastLogTime = 0;
    }


    // ------------------------------------------------
    // SYSTEM INITIALISATION
    // ------------------------------------------------
    void initializeSystem() {

      Serial.begin(9600);

      // Initialise hardware modules
      sensors.init();
      relay.init();
      ui.init();

      // Initialise internal EEPROM
      eepromManager.init();

      // Check SD card availability
      ui.updateScreen("Checking card", "Please wait");
      delay(1000);

      sdAvailable = sdManager.init();

      // SD logging is enabled only if the card is found
      sdLoggingEnabled = sdAvailable;

      if (sdAvailable) {

        ui.updateScreen("Card ready", "Starting system");
        delay(1500);

        Serial.println("[SYSTEM] SD card available.");
      }

      else {

        ui.updateScreen("Card not found", "Using built-in");
        delay(2000);

        ui.updateScreen("Limited storage", "Fewer records saved");
        delay(2500);

        Serial.println("[SYSTEM] SD unavailable.");
        Serial.println("[SYSTEM] Using EEPROM backup.");
      }


      // LOAD EXISTING OWNER PIN
      if (eepromManager.hasSavedPIN()) {

        auth.loadSavedHash(
          eepromManager.readPINHash()
        );

        isPinSetupComplete = true;
      }

      else {

        isPinSetupComplete = false;
      }


      // LOAD SAVED SAFETY CONFIGURATION
      float savedTemp;
      float savedCurrent;

      if (
        eepromManager.hasConfiguration() &&
        eepromManager.loadConfiguration(savedTemp, savedCurrent)
      ) {

        safety.setMaxTemp(savedTemp);
        safety.setMaxForwardCurrent(savedCurrent);

        Serial.println("[SYSTEM] Safety settings loaded.");
      }

      else {

        Serial.println("[SYSTEM] Using default safety settings.");
      }

      Serial.println("[SYSTEM] Initialisation complete.");
    }


    // ------------------------------------------------
    // MAIN DIAGNOSIS AND CONTROL LOOP
    // ------------------------------------------------
    void executeDiagnosisLoop() {

      // READ SENSOR VALUES
      float currentVoltage = sensors.getVoltage();
      float currentAmps = sensors.getCurrent();
      float panelTemp = sensors.getTemperature();


      // EVALUATE SENSOR READINGS
      bool dangerPresent = safety.evaluateReadings(
        panelTemp,
        currentAmps
      );


      // ------------------------------------------------
      // TELEMETRY LOGGING
      // ------------------------------------------------
      if (millis() - lastLogTime > 5000) {

        lastLogTime = millis();

        if (!faultManager.isHardLocked()) {

          // Save detailed telemetry to SD if enabled
          if (sdAvailable && sdLoggingEnabled) {

            sdManager.logTelemetry(
              NODE_ID,
              currentVoltage,
              currentAmps,
              panelTemp,
              faultManager.isFaultTriggered(),
              relay.getIsIsolated(),
              faultManager.getFaultCounter()
            );
          }

          else {

            // If SD is missing or has been ejected,
            // continue showing live data through Serial.
            Serial.print("[LIVE DATA] Voltage: ");
            Serial.print(currentVoltage);

            Serial.print(" V | Current: ");
            Serial.print(currentAmps);

            Serial.print(" A | Temperature: ");
            Serial.print(panelTemp);

            Serial.println(" C");
          }
        }
      }


      // ------------------------------------------------
      // FAULT DETECTION
      // ------------------------------------------------
      if (
        !faultManager.isFaultTriggered() &&
        !faultManager.isHardLocked()
      ) {

        if (dangerPresent) {

          String reason = "UNKNOWN";

          if (currentAmps < 0.0) {
            reason = "REVERSE CURRENT";
          }

          else if (
            currentAmps > safety.getMaxForwardCurrent()
          ) {
            reason = "OVERCURRENT SURGE";
          }

          else if (panelTemp > safety.getMaxTemp()) {
            reason = "THERMAL HOTSPOT";
          }


          // Register the fault
          faultManager.triggerFault(reason);

          // Isolate the circuit
          relay.openCircuit();

          // Record the fault
          recordMaintenance(
            faultManager.getFaultCounter(),
            "FAULT_DETECTED",
            reason
          );
        }
      }


      // ------------------------------------------------
      // AUTOMATIC RECOVERY AND HARD LOCKOUT
      // ------------------------------------------------
      if (
        faultManager.isFaultTriggered() &&
        !faultManager.isHardLocked()
      ) {

        if (!dangerPresent) {

          // Clear the temporary fault state
          faultManager.autoRecover();

          // Reconnect the circuit
          relay.closeCircuit();

          // Record recovery
          recordMaintenance(
            faultManager.getFaultCounter(),
            "AUTO_RECOVERED",
            "RELAY_CLOSED_SYSTEM_SAFE"
          );
        }

        else if (faultManager.checkEscalation()) {

          // Record hard lockout
          recordMaintenance(
            faultManager.getFaultCounter(),
            "HARD_LOCKOUT",
            "TECHNICIAN_REQUIRED"
          );
        }
      }


      // ------------------------------------------------
      // KEYPAD INPUT ROUTING
      // ------------------------------------------------
      char customKey = keypad.getKey();

      if (customKey) {


        // ================================================
        // STATE 1: FIRST-TIME PIN SETUP
        // ================================================
        if (!isPinSetupComplete) {

          if (
            customKey == '#' &&
            currentInput.length() >= 4
          ) {

            String newHash = auth.setupNewPIN(
              currentInput
            );

            eepromManager.savePINHash(newHash);

            isPinSetupComplete = true;
            currentInput = "";

            Serial.println("[SYSTEM] New PIN saved.");
          }

          else if (
            isDigit(customKey) &&
            currentInput.length() < 4
          ) {

            currentInput += customKey;
          }
        }


        // ================================================
        // STATE 2: OWNER LOGIN
        // ================================================
        else if (!isUnlocked) {

          if (customKey == '#') {

            bool authSuccess = auth.validatePIN(
              currentInput
            );

            recordAccess(
              authSuccess,
              "LOGIN_ATTEMPT"
            );

            if (authSuccess) {

              isUnlocked = true;

              Serial.println("[SYSTEM] Access granted.");
            }

            else {

              Serial.println("[SYSTEM] Access denied.");
            }

            currentInput = "";
          }

          else if (
            isDigit(customKey) &&
            currentInput.length() < 4
          ) {

            currentInput += customKey;
          }
        }


        // ================================================
        // STATE 3: CONFIGURATION MODE
        // ================================================
        else if (inConfigMode) {

          // Switch between wattage and voltage input
          if (customKey == '*') {

            typingVoltage = !typingVoltage;
          }

          // Save the configuration
          else if (customKey == '#') {

            // Do not save configuration while a fault
            // condition is currently present.
            if (dangerPresent) {

              Serial.println(
                "[SYSTEM] Cannot save settings while danger is present."
              );

              inConfigMode = false;
              inTechnicianMenu = true;
            }

            else {

              float w = inputWattage.toFloat();
              float v = inputVoltage.toFloat();

              if (w > 0 && v > 0) {

                float calculatedAmps = (w / v) * 1.25;

                // Temperature threshold remains fixed at 65 C
                float stdTemp = 65.0;

                // Save settings to EEPROM
                eepromManager.saveConfiguration(
                  stdTemp,
                  calculatedAmps
                );

                // Update active safety thresholds
                safety.setMaxTemp(stdTemp);

                safety.setMaxForwardCurrent(
                  calculatedAmps
                );

                // Record configuration change
                recordMaintenance(
                  0,
                  "CONFIGURATION_CHANGED",
                  "SAFETY_LIMITS_UPDATED"
                );

                Serial.println("[SYSTEM] Configuration updated.");
              }

              else {

                Serial.println(
                  "[SYSTEM] Invalid configuration values."
                );
              }

              inConfigMode = false;
              inTechnicianMenu = true;
            }
          }

          // Accept numeric input
          else if (isDigit(customKey)) {

            if (
              typingVoltage &&
              inputVoltage.length() < 3
            ) {

              inputVoltage += customKey;
            }

            else if (
              !typingVoltage &&
              inputWattage.length() < 4
            ) {

              inputWattage += customKey;
            }
          }
        }


        // ================================================
        // STATE 4: TECHNICIAN MENU
        // ================================================
        else if (inTechnicianMenu) {

          // Only allow configuration and reset actions
          // when the danger condition has cleared.
          if (!dangerPresent) {

            // B: Open configuration
            if (customKey == 'B') {

              enterConfigMode();
            }

            // C: Stop SD logging for card removal
            else if (customKey == 'C') {

              if (sdLoggingEnabled) {
                performSafeEject();
              }

              else {
                Serial.println("[SYSTEM] SD logging is already disabled.");
              }
            }

            // #: Technician reset, only during hard lockout
            else if (
              customKey == '#' &&
              faultManager.isHardLocked()
            ) {

              faultManager.techReset();

              relay.closeCircuit();

              recordMaintenance(
                faultManager.getFaultCounter(),
                "TECH_RESET_APPLIED",
                "RELAY_CLOSED_SYSTEM_SAFE"
              );

              inTechnicianMenu = false;

              Serial.println("[SYSTEM] Technician reset applied.");
            }
          }

          // *: Return to the previous screen
          if (customKey == '*') {

            inTechnicianMenu = false;
          }
        }


        // ================================================
        // STATE 5: HARD-LOCKOUT MAINTENANCE
        // ================================================
        else if (
          isUnlocked &&
          faultManager.isHardLocked()
        ) {

          // Only allow recovery actions when danger is absent
          if (!dangerPresent) {

            // #: Reset the system
            if (customKey == '#') {

              faultManager.techReset();

              relay.closeCircuit();

              recordMaintenance(
                faultManager.getFaultCounter(),
                "TECH_RESET_APPLIED",
                "RELAY_CLOSED_SYSTEM_SAFE"
              );

              Serial.println("[SYSTEM] Technician reset applied.");
            }

            // B: Open technician menu
            else if (customKey == 'B') {

              inTechnicianMenu = true;
            }

            // C: Safe SD eject
            else if (customKey == 'C') {

              if (sdLoggingEnabled) {
                performSafeEject();
              }
            }
          }

          // *: Owner logout
          if (customKey == '*') {

            isUnlocked = false;
            currentInput = "";

            inTechnicianMenu = false;

            recordAccess(
              true,
              "MANUAL_LOGOUT"
            );
          }
        }


        // ================================================
        // STATE 6: NORMAL OWNER DASHBOARD
        // ================================================
        else if (
          isUnlocked &&
          !faultManager.isHardLocked()
        ) {

          // *: Owner logout
          if (customKey == '*') {

            isUnlocked = false;
            currentInput = "";

            inTechnicianMenu = false;

            recordAccess(
              true,
              "MANUAL_LOGOUT"
            );
          }

          // A: Reset owner PIN
          else if (customKey == 'A') {

            eepromManager.clearSavedPIN();

            isPinSetupComplete = false;
            isUnlocked = false;

            currentInput = "";

            inTechnicianMenu = false;

            recordMaintenance(
              0,
              "PIN_RESET_INITIATED",
              "AWAITING_NEW_PIN"
            );
          }

          // B: Open technician menu
          else if (customKey == 'B') {

            inTechnicianMenu = true;

            Serial.println("[SYSTEM] Technician menu opened.");
          }
        }
      }


      // ------------------------------------------------
      // DISPLAY UPDATES
      // ------------------------------------------------
      if (millis() - lastDisplayUpdate > 200) {

        lastDisplayUpdate = millis();


        // First-time PIN setup
        if (!isPinSetupComplete) {

          ui.renderSetupScreen(currentInput);
        }

        // Owner login
        else if (!isUnlocked) {

          ui.renderLockScreen(currentInput);
        }

        // A hard lockout must take priority over ordinary
        // screens. The technician menu is shown only when
        // the danger condition has cleared.
        else if (faultManager.isHardLocked()) {

          if (inTechnicianMenu && !dangerPresent) {

            ui.renderTechnicianMenu(
              sdLoggingEnabled,
              true
            );
          }

          else {

            ui.renderFaultScreen(
              faultManager.getFaultReason(),
              dangerPresent
            );
          }
        }

        // Temporary fault/grace period
        else if (faultManager.isFaultTriggered()) {

          ui.renderGracePeriod(
            faultManager.getFaultReason()
          );
        }

        // Configuration screen
        else if (inConfigMode) {

          ui.renderConfigInput(
            inputWattage,
            inputVoltage,
            typingVoltage
          );
        }

        // Technician menu
        else if (inTechnicianMenu) {

          ui.renderTechnicianMenu(
            sdLoggingEnabled,
            false
          );
        }

        // Safe-eject confirmation
        else if (sdEjectConfirmed) {

          ui.renderSafeEject();
        }

        // Normal dashboard
        else {

          ui.renderDashboard(
            currentVoltage,
            currentAmps,
            panelTemp
          );
        }

        ui.displayUpdate();
      }
    }
};
