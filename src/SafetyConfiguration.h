#pragma once
#include <Arduino.h>

class SafetyConfiguration {
  private:
    float maxTempData;
    float maxReverseCurrent;
  public:
    SafetyConfiguration(float maxTemp, float maxRevCurrent) {
      maxTempData = maxTemp;
      maxReverseCurrent = maxRevCurrent;
    }
    bool evaluateReadings(float temp, float current) {
      return (current < maxReverseCurrent || temp > maxTempData);
    }
};