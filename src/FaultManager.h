#pragma once
#include <Arduino.h>

class FaultManager {
  private:
    bool faultTriggered;
    bool hardLocked;
    unsigned long faultStartTime;
    const unsigned long LOCKOUT_DELAY = 30000;
    String faultReason;
    unsigned int faultCounter;

  public:
    FaultManager() {
      faultTriggered = false; hardLocked = false;
      faultStartTime = 0; faultReason = ""; faultCounter = 0;
    }
    bool isFaultTriggered() { return faultTriggered; }
    bool isHardLocked() { return hardLocked; }
    String getFaultReason() { return faultReason; }
    unsigned int getFaultCounter() { return faultCounter; }

    void triggerFault(String reason) {
      faultTriggered = true;
      faultStartTime = millis();
      faultCounter++;
      faultReason = reason;
    }
    void autoRecover() { faultTriggered = false; }
    
    bool checkEscalation() {
      if (faultTriggered && !hardLocked && (millis() - faultStartTime > LOCKOUT_DELAY)) {
        hardLocked = true; return true;
      }
      return false;
    }
    void techReset() {
      hardLocked = false; faultTriggered = false; faultReason = "";
    }
};