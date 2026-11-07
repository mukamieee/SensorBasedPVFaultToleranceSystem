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

class SystemController {
  private:
    const String NODE_ID = "NODE_001";
    String currentInput;   
    bool isUnlocked; 
    bool isPinSetupComplete;
    unsigned long lastDisplayUpdate; 
    unsigned long lastLogTime; 

    // Menu State Trackers (Removed inMaintenanceMenu for simpler state routing)
    bool inConfigMode;
    bool typingVoltage;
    String inputWattage;
    String inputVoltage;

    SecurityAuthenticator auth;
    SafetyConfiguration safety;
    RelayModule relay;
    TelemetrySensors sensors;
    SDFileManager sdManager;
    UserInterface ui;
    KeypadManager keypad;
    FaultManager faultManager;

  public:
    SystemController() : 
      auth(), safety(65.0, 0.0), relay(22), 
      sensors(A0, A1, A2), sdManager(53) 
    {
      currentInput = ""; isUnlocked = false;
      isPinSetupComplete = false;
      inConfigMode = false; typingVoltage = false;
      inputWattage = ""; inputVoltage = "";
      lastDisplayUpdate = 0; lastLogTime = 0;
    }

    void initializeSystem() {
      Serial.begin(9600);
      sensors.init(); relay.init(); ui.init();

      ui.updateScreen("Booting SD..." , "Please wait");
      delay(1000); 

      if (!sdManager.init()) {
        ui.updateScreen("SD FATAL", "Insert Card & Reboot"); 
        while(1); 
      } 

      
      
      // CHECK DATABASE FOR EXISTING PIN
      if (sdManager.hasSavedPIN()) {
        auth.loadSavedHash(sdManager.readPINHash());
        isPinSetupComplete = true;
      } else {
        isPinSetupComplete = false; 
      }

      float savedTemp, savedCurrent;
      if (sdManager.hasConfiguration() && sdManager.loadConfiguration(savedTemp, savedCurrent)) {
        safety.setMaxTemp(savedTemp);
        safety.setMaxForwardCurrent(savedCurrent);
      }
    }

    void executeDiagnosisLoop() {
      float currentVoltage = sensors.getVoltage();
      float currentAmps = sensors.getCurrent();
      float panelTemp = sensors.getTemperature();
      bool dangerPresent = safety.evaluateReadings(panelTemp, currentAmps);

      // --- RELATIONAL TELEMETRY LOGGING ---
      if (millis() - lastLogTime > 5000) {
        lastLogTime = millis();
        if (!faultManager.isHardLocked()) {
          sdManager.logTelemetry(NODE_ID, currentVoltage, currentAmps, panelTemp, faultManager.isFaultTriggered(), relay.getIsIsolated(), faultManager.getFaultCounter());
        }
      }

      // --- ESCALATION STATE MACHINE ---
      if (!faultManager.isFaultTriggered() && !faultManager.isHardLocked()) {
        if (dangerPresent) {
          String reason = "UNKNOWN";
          if (currentAmps < 0.0) reason = "REVERSE CURRENT";
          else if (currentAmps > safety.getMaxForwardCurrent()) reason = "OVERCURRENT SURGE";
          else if (panelTemp > safety.getMaxTemp()) reason = "THERMAL HOTSPOT";

          faultManager.triggerFault(reason);
          relay.openCircuit(); 
          sdManager.logMaintenance(NODE_ID, faultManager.getFaultCounter(), "FAULT_DETECTED", reason);
        }
      }

      if (faultManager.isFaultTriggered() && !faultManager.isHardLocked()) {
        if (!dangerPresent) {
          faultManager.autoRecover();
          relay.closeCircuit();
          sdManager.logMaintenance(NODE_ID, faultManager.getFaultCounter(), "AUTO_RECOVERED", "RELAY_CLOSED_SYSTEM_SAFE");
        } 
        else if (faultManager.checkEscalation()) {
          sdManager.logMaintenance(NODE_ID, faultManager.getFaultCounter(), "HARD_LOCKOUT", "TECHNICIAN_REQUIRED");
        }
      }

      // --- KEYPAD ROUTING ---
      char customKey = keypad.getKey();
      if (customKey) {
        
        if (!isPinSetupComplete) {
          if (customKey == '#' && currentInput.length() >= 4) {
            sdManager.savePINHash(auth.setupNewPIN(currentInput));
            isPinSetupComplete = true; currentInput = "";
          } 
          else if (isDigit(customKey) && currentInput.length() < 4) currentInput += customKey; 
        }
        
        else if (!isUnlocked) {
          if (customKey == '#') {
            bool authSuccess = auth.validatePIN(currentInput);
            sdManager.logAccess(NODE_ID, authSuccess, "LOGIN_ATTEMPT"); 
            if (authSuccess) { isUnlocked = true; }
            currentInput = "";
          } 
          else if (isDigit(customKey) && currentInput.length() < 4) currentInput += customKey; 
        } 
        
        // STATE: CONFIGURING LIMITS (The 'B' Menu)
        else if (inConfigMode) {
          if (customKey == '*') {
            typingVoltage = !typingVoltage; 
          } 
          else if (customKey == '#') {
            float w = inputWattage.toFloat();
            float v = inputVoltage.toFloat();
            if (v > 0) { 
              float calculatedAmps = (w / v) * 1.25; 
              float stdTemp = 65.0; 
              sdManager.saveConfiguration(stdTemp, calculatedAmps);
              safety.setMaxTemp(stdTemp);
              safety.setMaxForwardCurrent(calculatedAmps);
            }
            inConfigMode = false; // Exits back to Fault Screen
          } 
          else if (isDigit(customKey)) {
            if (typingVoltage && inputVoltage.length() < 3) inputVoltage += customKey;
            else if (!typingVoltage && inputWattage.length() < 4) inputWattage += customKey;
          }
        }

        // FIX: STATE - MAINTENANCE MODE (System is Unlocked AND Broken)
        else if (isUnlocked && faultManager.isHardLocked()) {
          // Can only be used if danger is gone
          if (!dangerPresent) {
            if (customKey == '#') { 
              faultManager.techReset();
              relay.closeCircuit(); 
              sdManager.logMaintenance(NODE_ID, faultManager.getFaultCounter(), "TECH_RESET_APPLIED", "RELAY_CLOSED_SYSTEM_SAFE");
            } 
            else if (customKey == 'B') { 
              inConfigMode = true;
              inputWattage = ""; inputVoltage = ""; typingVoltage = false;
            } 
            else if (customKey == 'C') { 
              ui.renderSafeEject();
              sdManager.logMaintenance(NODE_ID, faultManager.getFaultCounter(), "SD_EJECT_MODE", "SYSTEM_HALTED");
              while(true) { delay(1000); } 
            }
          }
          // Allows user to lock the broken system without fixing it
          if (customKey == '*') {
            isUnlocked = false; currentInput = "";
            sdManager.logAccess(NODE_ID, true, "MANUAL_LOGOUT");
          }
        }

        // EXISTING: STATE - OWNER DASHBOARD (System is Unlocked and Healthy)
        else if (isUnlocked && !faultManager.isHardLocked()) {
          if (customKey == '*') {
            isUnlocked = false; currentInput = "";
            sdManager.logAccess(NODE_ID, true, "MANUAL_LOGOUT");
          } 
          else if (customKey == 'A') {
            isPinSetupComplete = false; isUnlocked = false; currentInput = "";
            sdManager.logMaintenance(NODE_ID, 0, "PIN_RESET_INITIATED", "AWAITING_NEW_PIN");
          }
        }
      }

      // --- UI UPDATES ---
      if (millis() - lastDisplayUpdate > 200) {
        lastDisplayUpdate = millis();
        
        if (!isPinSetupComplete) {
          ui.renderSetupScreen(currentInput); 
        } else if (!isUnlocked) {
          ui.renderLockScreen(currentInput);
        } else if (inConfigMode) {
          ui.renderConfigInput(inputWattage, inputVoltage, typingVoltage);
        } else if (faultManager.isHardLocked()) {
          // Maintenance options now render directly inside the fault screen
          ui.renderFaultScreen(faultManager.getFaultReason(), dangerPresent);
        } else if (faultManager.isFaultTriggered()) {
          ui.renderGracePeriod(faultManager.getFaultReason());
        } else {
          ui.renderDashboard(currentVoltage, currentAmps, panelTemp);
        }
        ui.displayUpdate();
      }
    } 
};