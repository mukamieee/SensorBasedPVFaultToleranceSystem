#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

class UserInterface {
  private:
    Adafruit_SSD1306 display;

  public:
    UserInterface() : display(128, 64, &Wire, -1) {}

    void init() {
      if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { for(;;); }
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(WHITE);
    }
    void updateScreen(String line1, String line2) {
      display.clearDisplay();
      display.setCursor(0, 10); display.println(line1);
      display.setCursor(0, 30); display.println(line2);
      display.display();
    }
    void renderLockScreen(String currentInput) {
      display.clearDisplay();
      display.setCursor(0, 10); display.println("System Locked");
      display.setCursor(0, 30); display.println("Enter Owner PIN:");
      String maskedPIN = "";
      for (unsigned int i = 0; i < currentInput.length(); i++) maskedPIN += "*";
      display.setCursor(0, 50); display.println(maskedPIN);
    }
    void renderFaultScreen(String faultReason, bool dangerPresent) {
      display.clearDisplay();
      display.setCursor(0, 0); display.println("!! HARDWARE FAULT !!");
      display.setCursor(0, 16); display.println(faultReason);
      if (dangerPresent) {
        display.setCursor(0, 36); display.println("DANGER STILL PRESENT");
        display.setCursor(0, 46); display.println("Please fix hardware.");
      } else {
        display.setCursor(0, 36); display.println("Hardware Safe.");
        display.setCursor(0, 46); display.println("Press # to Reset Relay");
      }
    }
    void renderGracePeriod(String faultReason) {
      display.clearDisplay();
      display.setCursor(0, 10); display.println("! ANOMALY DETECTED !");
      display.setCursor(0, 30); display.println(faultReason);
      display.setCursor(0, 50); display.println("Auto-Recovering...");
    }
    void renderDashboard(float currentVoltage, float currentAmps, float panelTemp) {
      display.clearDisplay();
      display.setCursor(0, 0);  display.println("--- PV MONITORING ---");
      display.setCursor(0, 20); display.print("Voltage: "); display.print(currentVoltage); display.println(" V");
      display.setCursor(0, 26); display.print("Current: "); display.print(currentAmps); display.println(" A");
      display.setCursor(0, 36); display.print("Temp:    "); display.print(panelTemp); display.println(" C");
      display.setCursor(0, 50); display.println("Press * to Lock");
    }
    void displayUpdate() { display.display(); }
};