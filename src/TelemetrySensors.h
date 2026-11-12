
#pragma once
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <math.h>

class TelemetrySensors {
  private:
    int voltagePin, currentPin, tempPin;
    float currentZero = 512.0;

    OneWire oneWire;
    DallasTemperature tempSensor;

    static const uint8_t MAX_TEMP_SENSORS = 4;

    uint8_t tempSensorCount = 0;

    float temperatures[MAX_TEMP_SENSORS] = {
      -127.0, -127.0, -127.0, -127.0
    };

  public:
    TelemetrySensors(int vPin, int iPin, int tPin)
      : oneWire(tPin), tempSensor(&oneWire) {
      voltagePin = vPin;
      currentPin = iPin;
      tempPin = tPin;
    }

    void init() {
      analogRead(voltagePin);

      tempSensor.begin();

      // Detect connected DS18B20 probes
      tempSensorCount = tempSensor.getDeviceCount();

      if (tempSensorCount > MAX_TEMP_SENSORS) {
        tempSensorCount = MAX_TEMP_SENSORS;
      }

      // Allow the ACS712 output to stabilize
      delay(500);

      // Calibrate the zero-current reading
      long total = 0;

      for (int i = 0; i < 500; i++) {
        total += analogRead(currentPin);
        delay(2);
      }

      currentZero = total / 500.0;
    }

    float getVoltage() {
      return analogRead(voltagePin) * (24.0 / 1023.0);
    }

    float getCurrent() {
      int raw = analogRead(currentPin);

      float voltageDifference =
        (raw - currentZero) * (5.0 / 1023.0);

      float current = voltageDifference / 0.100;

      // Ignore small fluctuations around zero
      if (fabs(current) < 0.20) {
        return 0.0;
      }

      return current;
    }

    // Read all connected temperature probes
    void updateTemperatures() {
      tempSensor.requestTemperatures();

      for (uint8_t i = 0; i < MAX_TEMP_SENSORS; i++) {

        if (i < tempSensorCount) {
          float reading = tempSensor.getTempCByIndex(i);

          if (reading == DEVICE_DISCONNECTED_C) {
            temperatures[i] = -127.0;
          } else {
            temperatures[i] = reading;
          }

        } else {
          temperatures[i] = -127.0;
        }
      }
    }

    // Retrieve a specific probe's temperature
    float getTemperature(uint8_t index) {
      if (index >= tempSensorCount ||
          index >= MAX_TEMP_SENSORS) {
        return -127.0;
      }

      return temperatures[index];
    }

    // Backward compatibility with existing code
    float getTemperature() {
      updateTemperatures();
      return getTemperature(0);
    }

    // Number of detected probes
    uint8_t getTemperatureCount() {
      return tempSensorCount;
    }
};
