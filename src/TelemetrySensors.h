#pragma once
#include <Arduino.h>

class TelemetrySensors {
  private:
    int voltagePin, currentPin, tempPin;

  public:
    TelemetrySensors(int vPin, int iPin, int tPin) {
      voltagePin = vPin; currentPin = iPin; tempPin = tPin;
    }
    void init() {
      analogRead(voltagePin); analogRead(currentPin); analogRead(tempPin);
      delay(100);
    }
    float getVoltage() {
      return analogRead(voltagePin) * (24.0 / 1023.0);
    }
    float getCurrent() {
      return (analogRead(currentPin) - 512) * (5.0 / 512.0); 
    }
    float getTemperature() {
      int rawTemp = analogRead(tempPin);
      if (rawTemp <= 0 || rawTemp >= 1023) return 24.0; 
      float resistance = 10000.0 / (1023.0 / rawTemp - 1.0);
      return 1.0 / (log(resistance / 10000.0) / 3950.0 + 1.0 / 298.15) - 273.15;
    }
};