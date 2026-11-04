#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

class SDFileManager {
  private:
    int csPin;
  public:
    SDFileManager(int pin) { csPin = pin; }

    bool init() {
      pinMode(csPin, OUTPUT);
      return SD.begin(csPin);
    }

    // ERD Entity: ACCESS_LOG
    void logAccess(String nodeId, bool wasSuccessful, String action) {
      File logFile = SD.open("access.csv", FILE_WRITE);
      String authStatus = wasSuccessful ? "GRANTED" : "DENIED";
      String logEntry = String(millis()) + "," + nodeId + "," + authStatus + "," + action;
      if (logFile) { logFile.println(logEntry); logFile.close(); }
      Serial.println("[ACCESS LOG] " + logEntry);
    }

    // ERD Entity: TELEMETRY_LOG 
    void logTelemetry(String nodeId, float v, float i, float t, bool isFault, bool isIsolated, unsigned int activeFaultId) {
      File dataFile = SD.open("data.csv", FILE_WRITE); 
      String faultStr = isFault ? "TRUE" : "FALSE";
      String relayStr = isIsolated ? "OPEN" : "CLOSED";
      String dataEntry = String(millis()) + "," + nodeId + "," + String(v) + "," + String(i) + "," + String(t) + "," + faultStr + "," + relayStr + "," + String(activeFaultId);
      if (dataFile) { dataFile.println(dataEntry); dataFile.close(); }
      Serial.println("[DATA LOG] " + dataEntry);
    }

    // ERD Entity: MAINTENANCE_RECORD
    void logMaintenance(String nodeId, unsigned int faultId, String actionTaken, String systemState) {
      File eventFile = SD.open("maint.csv", FILE_WRITE); 
      String eventEntry = String(millis()) + "," + nodeId + "," + String(faultId) + "," + actionTaken + "," + systemState;
      if (eventFile) { eventFile.println(eventEntry); eventFile.close(); }
      Serial.println("[MAINTENANCE LOG] " + eventEntry);
    }

    // Dynamic PIN Management
    bool hasSavedPIN() {
      return SD.exists("pin.csv");
    }

    String readPINHash() {
      File pinFile = SD.open("pin.csv", FILE_READ);
      String hashStr = "";
      if (pinFile) {
        hashStr = pinFile.readStringUntil('\n');
        hashStr.trim();
        pinFile.close();
      }
      return hashStr;
    }

    void savePINHash(String hashStr) {
      if (SD.exists("pin.csv")) SD.remove("pin.csv"); 
      File pinFile = SD.open("pin.csv", FILE_WRITE);
      if (pinFile) {
        pinFile.println(hashStr);
        pinFile.close();
      }
      Serial.println("[SECURITY] New PIN Hash saved.");
    }
};