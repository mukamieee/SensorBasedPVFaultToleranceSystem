#pragma once
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
    String currentInput;   
    bool isUnlocked;       
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
      auth("1234"), safety(60.0, 0.0), relay(22), 
      sensors(A0, A1, A2), sdManager(53) 
    {
      currentInput = ""; isUnlocked = false;
      lastDisplayUpdate = 0; lastLogTime = 0;
    }

    void initializeSystem() {
      Serial.begin(9600);
      sensors.init(); relay.init(); ui.init();

      ui.updateScreen("Booting SD..." , "Please wait");
      delay(1000); 

      if (!sdManager.init()) {
        ui.updateScreen("SD ERROR", "Initialization failed"); delay(2000);
      } else {
        ui.updateScreen("SD READY", "Card initialized"); delay(2000);
      }
      ui.updateScreen("System Locked", "Enter PIN:");
    }

    void executeDiagnosisLoop() {
      float currentVoltage = sensors.getVoltage();
      float currentAmps = sensors.getCurrent();
      float panelTemp = sensors.getTemperature();
      bool dangerPresent = safety.evaluateReadings(panelTemp, currentAmps);

      // --- LOGGING ---
      if (millis() - lastLogTime > 5000) {
        lastLogTime = millis();
        sdManager.logTelemetry(currentVoltage, currentAmps, panelTemp);
      }

      // --- ESCALATION STATE MACHINE ---
      if (!faultManager.isFaultTriggered() && !faultManager.isHardLocked()) {
        if (dangerPresent) {
          String reason = (currentAmps < 0.0) ? "REVERSE CURRENT" : "THERMAL HOTSPOT";
          faultManager.triggerFault(reason);
          relay.openCircuit(); 
          sdManager.logEvent("FAULT_DETECTED", "ID:" + String(faultManager.getFaultCounter()) + " Reason: " + reason + " | Relay OPENED");
        }
      }

      if (faultManager.isFaultTriggered() && !faultManager.isHardLocked()) {
        if (!dangerPresent) {
          faultManager.autoRecover();
          relay.closeCircuit();
          sdManager.logEvent("AUTO_RECOVERED", "ID:" + String(faultManager.getFaultCounter()) + " Danger cleared | Relay CLOSED");
        } 
        else if (faultManager.checkEscalation()) {
          sdManager.logEvent("HARD_LOCKOUT", "ID:" + String(faultManager.getFaultCounter()) + " Grace period expired. Tech required.");
        }
      }

      // --- KEYPAD ROUTING ---
      char customKey = keypad.getKey();
      if (customKey) {
        if (!isUnlocked) {
          if (customKey == '#') {
            bool authSuccess = auth.validatePIN(currentInput);
            sdManager.logAccess(authSuccess);
            if (authSuccess) isUnlocked = true;
            currentInput = "";
          } 
          else if (customKey != '*') {
            if (currentInput.length() < 4) currentInput += customKey; 
          }
        } 
        else {
          if (customKey == '*') {
            isUnlocked = false; currentInput = "";
          } 
          else if (customKey == '#' && faultManager.isHardLocked() && !dangerPresent) {
            faultManager.techReset();
            relay.closeCircuit(); 
            sdManager.logEvent("TECH_RESET", "ID:" + String(faultManager.getFaultCounter()) + " PIN verified. Hardware safe | Relay CLOSED");
          }
        }
      }

      // --- UI UPDATES ---
      if (millis() - lastDisplayUpdate > 200) {
        lastDisplayUpdate = millis();
        if (!isUnlocked) {
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