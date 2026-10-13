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
    byte rowPins[4] = {9, 8, 7, 6};
    byte colPins[4] = {5, 4, 3, 2};
    Keypad customKeypad;

  public:
    KeypadManager() : customKeypad(makeKeymap(keys), rowPins, colPins, 4, 4) {}
    char getKey() { return customKeypad.getKey(); }
};