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
    void logAccess(bool wasSuccessful) {
      File logFile = SD.open("access.txt", FILE_WRITE);
      String logEntry = "Time(ms):" + String(millis()) + ", Auth_Status: " + (wasSuccessful ? "GRANTED" : "DENIED");
      if (logFile) { logFile.println(logEntry); logFile.close(); }
      Serial.println("[ACCESS LOG] " + logEntry);
    }
    void logTelemetry(float v, float i, float t) {
      File dataFile = SD.open("data.txt", FILE_WRITE);
      String dataEntry = "Time:" + String(millis()) + "ms, V:" + String(v) + ", I:" + String(i) + ", T:" + String(t);
      if (dataFile) { dataFile.println(dataEntry); dataFile.close(); }
      Serial.println("[DATA LOG] " + dataEntry);
    }
    void logEvent(String eventType, String details) {
      File eventFile = SD.open("events.txt", FILE_WRITE);
      String eventEntry = "Time:" + String(millis()) + "ms [" + eventType + "] " + details;
      if (eventFile) { eventFile.println(eventEntry); eventFile.close(); }
      Serial.println("[EVENT LOG] " + eventEntry);
    }
};