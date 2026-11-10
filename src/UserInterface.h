#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

class UserInterface {
  private:
    Adafruit_SSD1306 display;

  public:
    UserInterface() : display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET) {}

    void init() {
      if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("SSD1306 allocation failed"));
        for(;;); 
      }
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
    }

    void updateScreen(String line1, String line2) {
      display.clearDisplay();
      display.setCursor(0, 10);
      display.println(line1);
      display.println(line2);
      display.display();
    }

    void renderSetupScreen(String currentInput) {
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("--- SYSTEM SETUP ---");
      display.println("Create 4-Digit PIN:");
      display.setCursor(0, 25);
      
      String hiddenPin = "";
      // FIX: Changed 'int' to 'unsigned int' to clear the compiler warning
      for (unsigned int i = 0; i < currentInput.length(); i++) {
        hiddenPin += "* ";
      }
      display.println(hiddenPin);
      display.setCursor(0, 50);
      display.println("Press # to Save");
    }

    void renderLockScreen(String currentInput) {
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("--- SYSTEM LOCKED ---");
      display.println("Enter PIN:");
      display.setCursor(0, 25);
      
      String hiddenPin = "";
      // FIX: Changed 'int' to 'unsigned int' to clear the compiler warning
      for (unsigned int i = 0; i < currentInput.length(); i++) {
        hiddenPin += "* ";
      }
      display.println(hiddenPin);
    }

    // FIX: Merged the Maintenance Menu directly into the Fault Screen
    void renderFaultScreen(String faultReason, bool dangerPresent) {
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("!! FAULT LOCKOUT !!");
      display.println(faultReason);
      
      if (dangerPresent) {
        display.setCursor(0, 30);
        display.println("DANGER STILL PRESENT");
      } else {
        display.setCursor(0, 25);
        display.println("#:Reset  B:Limits");
        display.println("C:Safe SD Eject");
      }
    }

    void renderGracePeriod(String faultReason) {
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("! ANOMALY DETECTED !");
      display.println(faultReason);
      display.setCursor(0, 30);
      display.println("Auto-Recovering...");
    }

    
  void renderDashboard(float v, float i, float t) {

    display.clearDisplay();
    display.setCursor(0, 0);

    display.println("--- PV MONITORING ---");

    display.print("Voltage : ");
    display.print(v);
    display.println(" V");

    display.print("Current : ");
    display.print(i);
    display.println(" A");

    display.print("Temp    : ");
    display.print(t);
    display.println(" C");

    display.println("B: Technician Menu");
  }


    void renderConfigInput(String inputW, String inputV, bool typingVoltage) {
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("--- PANEL SETUP ---");
      
      display.print("Wattage: ");
      display.print(inputW);
      if (!typingVoltage) display.print("_"); 
      display.println(" W");

      display.print("Voltage: ");
      display.print(inputV);
      if (typingVoltage) display.print("_");
      display.println(" V");

      display.setCursor(0, 45);
      display.println("* switch | # save");
    }

    void renderSafeEject() {
      display.clearDisplay();
      display.setCursor(0, 10);
      display.println("SD CARD SAFE");
      display.println("TO REMOVE.");
      display.setCursor(0, 40);
      display.println("REBOOT TO RESUME");
      display.display(); 
    }

    void displayUpdate() { display.display(); }

    
  // Displays the technician menu
  void renderTechnicianMenu(bool sdLoggingEnabled, bool hardLocked) {

    display.clearDisplay();
    display.setCursor(0, 0);

    display.println("--- TECH MENU ---");
    display.println("B: Configure limits");

    if (sdLoggingEnabled) {
      display.println("C: Remove SD card");
    } else {
      display.println("C: SD already removed");
    }

    if (hardLocked) {
      display.println("#: Reset system");
    }

    display.println("*: Back");
    display.display();
  }

};