#pragma once
#include <Arduino.h>

class SecurityAuthenticator {
  private:
    String masterPIN;
  public:
    SecurityAuthenticator(String pin) { 
      masterPIN = pin; 
    }
    bool validatePIN(String inputPIN) { 
      return inputPIN == masterPIN; 
    }
};