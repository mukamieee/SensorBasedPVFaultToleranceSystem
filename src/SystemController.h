#pragma once //called during compilatiton so header is called once
#include <Arduino.h>

// Import all system modules
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

    // Instantiate modular components
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
      auth(), 
      safety(60.0, 0.0), relay(22), 
      sensors(A0, A1, A2), sdManager(53) 
    {
      currentInput = ""; 
      isUnlocked = false;
      isPinSetupComplete = false;
      lastDisplayUpdate = 0; 
      lastLogTime = 0;
    }

    void initializeSystem() {
      Serial.begin(9600);
      sensors.init(); relay.init(); ui.init();

      ui.updateScreen("Booting SD..." , "Please wait");
      delay(1000); 

      if (!sdManager.init()) {
        ui.updateScreen("SD FATAL", "Insert Card & Reboot"); 
        while(1); // Halt system if no SD is present (Cannot run without DB)
      } 

      SD.remove("pin.csv");
      
      // CHECK DATABASE FOR EXISTING PIN
      if (sdManager.hasSavedPIN()) {
        auth.loadSavedHash(sdManager.readPINHash());
        isPinSetupComplete = true; // Skip setup screen
        ui.updateScreen("System Locked", "Enter PIN:");
      } else {
        isPinSetupComplete = false; // Trigger setup screen
      }
    }

    void executeDiagnosisLoop() {
      float currentVoltage = sensors.getVoltage();
      float currentAmps = sensors.getCurrent();
      float panelTemp = sensors.getTemperature();
      bool dangerPresent = safety.evaluateReadings(panelTemp, currentAmps);

      
      // --- RELATIONAL TELEMETRY LOGGING (Updated for ERD) ---
      if (millis() - lastLogTime > 5000) {
        lastLogTime = millis();
        
        // NEW LOGIC: Only log telemetry if the system is NOT in a hard lockout
        if (!faultManager.isHardLocked()) {
          sdManager.logTelemetry(NODE_ID, currentVoltage, currentAmps, panelTemp, faultManager.isFaultTriggered(), relay.getIsIsolated(), faultManager.getFaultCounter());
        }
      }

      // --- ESCALATION STATE MACHINE (Updated to logMaintenance for ERD) ---
      if (!faultManager.isFaultTriggered() && !faultManager.isHardLocked()) {
        if (dangerPresent) {
          String reason = (currentAmps < 0.0) ? "REVERSE CURRENT" : "THERMAL HOTSPOT";
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
        
        // NEW: If system is brand new and needs a PIN
        if (!isPinSetupComplete) {
          if (customKey == '#' && currentInput.length() >= 4) {
            String newHash = auth.setupNewPIN(currentInput);
            sdManager.savePINHash(newHash); // Write to database
            isPinSetupComplete = true;
            currentInput = "";
          } 
          else if (customKey != '*' && customKey != '#') {
            if (currentInput.length() < 4) currentInput += customKey; 
          }
        }
        // EXISTING: Normal Locked State
        else if (!isUnlocked) {
          if (customKey == '#') {
            bool authSuccess = auth.validatePIN(currentInput);
            sdManager.logAccess(NODE_ID, authSuccess, "LOGIN_ATTEMPT"); 
            if (authSuccess) isUnlocked = true;
            currentInput = "";
          } 
          else if (customKey != '*') {
            if (currentInput.length() < 4) currentInput += customKey; 
          }
        } 
        // EXISTING: Unlocked State
        else {
          if (customKey == '*') {
            isUnlocked = false; currentInput = "";
            sdManager.logAccess(NODE_ID, true, "MANUAL_LOGOUT");
          } 
          else if (customKey == '#' && faultManager.isHardLocked() && !dangerPresent) {
            faultManager.techReset();
            relay.closeCircuit(); 
            sdManager.logMaintenance(NODE_ID, faultManager.getFaultCounter(), "TECH_RESET_APPLIED", "RELAY_CLOSED_SYSTEM_SAFE");
          }
        }
      }

      // --- UI UPDATES ---
      if (millis() - lastDisplayUpdate > 200) {
        lastDisplayUpdate = millis();
        
        if (!isPinSetupComplete) {
          ui.renderSetupScreen(currentInput); // Show new setup screen
        } else if (!isUnlocked) {
          ui.renderLockScreen(currentInput);
        } else if (faultManager.isHardLocked()) {
          ui.renderFaultScreen(faultManager.getFaultReason(), dangerPresent);
        } else if (faultManager.isFaultTriggered() && !faultManager.isHardLocked()) {
          ui.renderGracePeriod(faultManager.getFaultReason());
        } else {
          ui.renderDashboard(currentVoltage, currentAmps, panelTemp);
        }
        ui.displayUpdate();
      }
    } 
}; 