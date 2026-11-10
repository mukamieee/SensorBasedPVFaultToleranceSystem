
#pragma once
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <math.h>

class TelemetrySensors {
  private:
    int voltagePin, currentPin, tempPin;

    OneWire oneWire;
    DallasTemperature tempSensor;

  public:
    TelemetrySensors(int vPin, int iPin, int tPin)
      : oneWire(tPin), tempSensor(&oneWire) {
      voltagePin = vPin;
      currentPin = iPin;
      tempPin = tPin;
    }

    void init() {
      analogRead(voltagePin);
      analogRead(currentPin);

      tempSensor.begin();
      delay(100);
    }

    float getVoltage() {
      return analogRead(voltagePin) * (24.0 / 1023.0);
    }

    float getCurrent() {
      return (analogRead(currentPin) - 512) * (5.0 / 512.0);
    }

    float getTemperature() {
      tempSensor.requestTemperatures();

      float temperature = tempSensor.getTempCByIndex(0);

      if (temperature == DEVICE_DISCONNECTED_C) {
        return -127.0;
      }

      return temperature;
    }
};
