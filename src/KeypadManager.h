#pragma once
#include <Arduino.h>
#include <Keypad.h>

class KeypadManager {
  private:
    char keys[4][4] = {
      {'1','2','3','A'},
      {'4','5','6','B'},
      {'7','8','9','C'},
      {'*','0','#','D'}
    };
    byte rowPins[4] = {2, 3, 4, 5};
    byte colPins[4] = {6, 7, 8, 9};
    Keypad customKeypad;

  public:
    KeypadManager() : customKeypad(makeKeymap(keys), rowPins, colPins, 4, 4) {}
    char getKey() { return customKeypad.getKey(); }
};