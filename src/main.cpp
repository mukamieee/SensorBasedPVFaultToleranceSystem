#include <Arduino.h>
#include "SystemController.h"

SystemController masterNode; 

void setup() {
  masterNode.initializeSystem();
}

void loop() {
  masterNode.executeDiagnosisLoop();
}