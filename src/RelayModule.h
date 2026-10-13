#pragma once
#include <Arduino.h>

class RelayModule {
  private:
    int controlPin;
  public:
    RelayModule(int pin) { controlPin = pin; }
    void init() {
      pinMode(controlPin, OUTPUT);
      digitalWrite(controlPin, HIGH);
    }
    void openCircuit() { digitalWrite(controlPin, LOW); }
    void closeCircuit() { digitalWrite(controlPin, HIGH); }
};