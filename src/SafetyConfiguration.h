#pragma once
#include <Arduino.h>

class SafetyConfiguration {
  private:
    float maxTempLimit;
    float maxReverseCurrent;
    float maxForwardCurrent; // NEW: Dynamically calculated based on Wattage/Voltage

  public:
    SafetyConfiguration(float temp, float reverse) {
      maxTempLimit = temp;
      maxReverseCurrent = reverse;
      maxForwardCurrent = 10.0; // Safe initial default before SD load
    }

    // New Setters for our config file
    void setMaxTemp(float temp) { maxTempLimit = temp; }
    void setMaxForwardCurrent(float current) { maxForwardCurrent = current; }
    
    // New Getters for our UI/Logging
    float getMaxTemp() { return maxTempLimit; }
    float getMaxForwardCurrent() { return maxForwardCurrent; }

    bool evaluateReadings(float temp, float current) {
      // Triggers if too hot, reverse current, OR exceeds the new dynamic forward current
      return (temp > maxTempLimit) || (current < maxReverseCurrent) || (current > maxForwardCurrent);
    }
};