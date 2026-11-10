
#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

class SDFileManager {

  private:
    int csPin;
    bool cardInitialized;
    bool loggingEnabled;
    

  public:

    SDFileManager(int pin) {
      csPin = pin;
      cardInitialized = false;
      loggingEnabled = false;
    }

    // Initialise the SD card
    bool init() {

      pinMode(csPin, OUTPUT);
      digitalWrite(csPin, HIGH);

      cardInitialized = SD.begin(csPin);
      loggingEnabled = cardInitialized;

      return cardInitialized;
    }

    // Check whether SD logging is currently allowed
    bool isLoggingEnabled() {
      return loggingEnabled;
    }

    // Stop all future SD logging
    void stopLogging() {

      // Disable logging before any further file operation
      loggingEnabled = false;

      // Leave the SD card deselected on the SPI bus
      digitalWrite(csPin, HIGH);

      Serial.println("[SD] Logging stopped. Card may be removed.");
    }

    // ERD Entity: ACCESS_LOG
    bool logAccess(String nodeId,
                   bool wasSuccessful,
                   String action) {

      if (!loggingEnabled) return false;

      File logFile = SD.open("access.csv", FILE_WRITE);

      if (!logFile) {
        Serial.println("[SD] Could not open access.csv");
        return false;
      }

      String authStatus = wasSuccessful ? "GRANTED" : "DENIED";

      String logEntry = String(millis()) + "," +
                        nodeId + "," +
                        authStatus + "," +
                        action;

      logFile.println(logEntry);
      logFile.close();

      Serial.println("[ACCESS LOG] " + logEntry);
      return true;
    }

    // ERD Entity: TELEMETRY_LOG
    bool logTelemetry(String nodeId,
                      float v,
                      float i,
                      float t,
                      bool isFault,
                      bool isIsolated,
                      unsigned int activeFaultId) {

      if (!loggingEnabled) return false;

      File dataFile = SD.open("data.csv", FILE_WRITE);

      if (!dataFile) {
        Serial.println("[SD] Could not open data.csv");
        return false;
      }

      String faultStr = isFault ? "TRUE" : "FALSE";
      String relayStr = isIsolated ? "OPEN" : "CLOSED";

      String dataEntry = String(millis()) + "," +
                         nodeId + "," +
                         String(v) + "," +
                         String(i) + "," +
                         String(t) + "," +
                         faultStr + "," +
                         relayStr + "," +
                         String(activeFaultId);

      dataFile.println(dataEntry);
      dataFile.close();

      Serial.println("[DATA LOG] " + dataEntry);
      return true;
    }

    // ERD Entity: MAINTENANCE_RECORD
    bool logMaintenance(String nodeId,
                        unsigned int faultId,
                        String actionTaken,
                        String systemState) {

      if (!loggingEnabled) return false;

      File eventFile = SD.open("maint.csv", FILE_WRITE);

      if (!eventFile) {
        Serial.println("[SD] Could not open maint.csv");
        return false;
      }

      String eventEntry = String(millis()) + "," +
                          nodeId + "," +
                          String(faultId) + "," +
                          actionTaken + "," +
                          systemState;

      eventFile.println(eventEntry);
      eventFile.close();

      Serial.println("[MAINTENANCE LOG] " + eventEntry);
      return true;
    }
};
