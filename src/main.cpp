#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Keypad.h>
#include <SPI.h>
#include <SD.h>

// --- OLED CONFIGURATION ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// --- KEYPAD CONFIGURATION ---
const byte ROWS = 4; 
const byte COLS = 4; 

char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = {9, 8, 7, 6}; 
byte colPins[COLS] = {5, 4, 3, 2}; 
Keypad customKeypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// --- HARDWARE PINS ---
const int SD_CS_PIN = 53; // Chip Select pin for the SD Card
const int RELAY_PIN = 22;     // The pin controlling the electromechanical relay
// --- SENSOR PINS ---
const int VOLTAGE_PIN = A0; // Simulates the voltage divider
const int CURRENT_PIN = A1; // Simulates the INA219 current sensor
const int TEMP_PIN = A2;    // Simulates the thermal array


// --- SECURITY LOGIC ---
String masterPIN = "1234";  // The hardcoded singleton owner PIN
String currentInput = "";   // The buffer holding the current key presses
bool isUnlocked = false;    // State machine flag

// --- NEW TIME-BASED FAULT STATES ---
bool isFaultTriggered = false; // Tracks if the system is in an emergency lockout
bool isHardLocked = false;
unsigned long faultStartTime = 0;
const unsigned long LOCKOUT_DELAY = 30000; // 30 seconds (Change to minutes for real world)
String faultReason = "";   // Stores the specific reason for the lockout    
unsigned long lastDisplayUpdate = 0; // Timer for non-blocking UI

unsigned long lastLogTime = 0; // Timer for 5-second telemetry logging
unsigned int faultCounter = 0; // Generates unique IDs for each anomaly event

// --- SENSOR ABSTRACTION FUNCTIONS ---
float getVoltage() {
  // SIMULATION: Scales the 0-1023 dial to a 0.0V - 24.0V range
  int rawV = analogRead(VOLTAGE_PIN);
  return rawV * (24.0 / 1023.0);
}
float getCurrent() {
  // SIMULATION: Scales the 0-1023 dial to a -5.0A to +5.0A range
  // Center dial (512) = 0 Amps. Below center = Reverse Current Leakage.
  int rawI = analogRead(CURRENT_PIN);
  return (rawI - 512) * (5.0 / 512.0); 
  
  // REAL WORLD: return ina219.getCurrent_mA() / 1000.0;
}
float getTemperature() {
  int rawTemp = analogRead(TEMP_PIN);
  if (rawTemp <= 0 || rawTemp >= 1023) return 24.0; // Fallback for analog glitches
  
  // Accurate Steinhart-Hart equation for the Wokwi NTC thermistor
  float resistance = 10000.0 / (1023.0 / rawTemp - 1.0);
  float celsius = 1.0 / (log(resistance / 10000.0) / 3950.0 + 1.0 / 298.15) - 273.15;
  return celsius;
}


void updateScreen(String line1, String line2);
void logAccess(bool wasSuccessful);
void logTelemetry(float v, float i, float t);
void logEvent(String eventType, String details);

void setup() {
  Serial.begin(9600); // Initialize serial communication for debugging
  
  analogRead(VOLTAGE_PIN);
  analogRead(CURRENT_PIN);
  analogRead(TEMP_PIN);
  delay(100);

  pinMode(SD_CS_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT); // Initialize the relay control pin
  // Power up the relay (HIGH = Circuit Closed / Power Flowing)
  digitalWrite(RELAY_PIN, HIGH);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for(;;); 
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  

  updateScreen("Booting SD..." , "Please wait");
  delay(1000); // Wait for 1 second to allow the SD card to initialize

  if (!SD.begin(SD_CS_PIN)) {
    updateScreen("SD ERROR", "Initialization failed");
    delay(2000);
  } else {
    updateScreen("SD READY", "Card initialized");
    delay(2000);
  }

  updateScreen("System Locked", "Enter PIN:");
}

void loop() {

  // TASK 1: UNCONDITIONAL BACKGROUND MONITORING
  
  float currentVoltage = getVoltage();
  float currentAmps = getCurrent();
  float panelTemp = getTemperature();

  // Evaluate if physical danger is currently present
  bool dangerPresent = (currentAmps < 0.0 || panelTemp > 60.0);

  // --- CONTINUOUS TELEMETRY LOGGING (Every 5 Seconds) ---
  if (millis() - lastLogTime > 5000) {
    lastLogTime = millis();
    logTelemetry(currentVoltage, currentAmps, panelTemp);
  }

  // STAGE 1: Detect new anomaly and start the Grace Period
  if (!isFaultTriggered && !isHardLocked) {
    if (dangerPresent) {
      isFaultTriggered = true;
      faultStartTime = millis(); 
      faultCounter++; // Generate a new unique Fault ID
      faultReason = (currentAmps < 0.0) ? "REVERSE CURRENT" : "THERMAL HOTSPOT";
      digitalWrite(RELAY_PIN, LOW); // Instantly protect the circuit
      
      // THIS IS THE LINE YOU MISSED:
      logEvent("FAULT_DETECTED", "ID:" + String(faultCounter) + " Reason: " + faultReason + " | Relay OPENED");
    }
  }

  // STAGE 2: Monitor the Grace Period
  if (isFaultTriggered && !isHardLocked) {
    if (!dangerPresent) {
      // The anomaly (e.g. passing cloud) cleared quickly! Auto-recover.
      isFaultTriggered = false;
      digitalWrite(RELAY_PIN, HIGH);
      
      // THIS IS THE LINE YOU MISSED:
      logEvent("AUTO_RECOVERED", "ID:" + String(faultCounter) + " Danger cleared | Relay CLOSED");
    } 
    else if (millis() - faultStartTime > LOCKOUT_DELAY) {
      // The anomaly persisted for > 30 seconds. Escalate to Hard Lockout.
      isHardLocked = true;
      
      // THIS IS THE LINE YOU MISSED:
      logEvent("HARD_LOCKOUT", "ID:" + String(faultCounter) + " Grace period expired. Technician required.");
    }
  }

  

  // TASK 2: FOREGROUND KEYPAD LISTENER
  char customKey = customKeypad.getKey();
  
  if (customKey) {
    if (!isUnlocked) {
      // STATE: SYSTEM LOCKED (Only accept PIN)
      if (customKey == '#') {
        if (currentInput == masterPIN) {
          isUnlocked = true;
          logAccess(true);
        } else {
          logAccess(false);
        }
        currentInput = "";
      } 
      else if (customKey != '*') {
        if (currentInput.length() < 4) currentInput += customKey; 
      }
    } 
    else {
      // STATE: SYSTEM UNLOCKED
      if (customKey == '*') {
        isUnlocked = false; // Manually lock the screen
        currentInput = "";
      } 
      else if (customKey == '#' && isHardLocked && !dangerPresent) {
        // Owner resets the relay AFTER the physical anomaly is fixed
        isHardLocked = false;
        isFaultTriggered = false;
        faultReason = "";
        digitalWrite(RELAY_PIN, HIGH); 

        logEvent("TECH_RESET", "ID:" + String(faultCounter) + " PIN verified. Hardware safe | Relay CLOSED");

      }
    }
  }

  // TASK 3: UI SCREEN UPDATER (Non-Blocking)
  // Update the screen every 200 milliseconds to prevent flickering
if (millis() - lastDisplayUpdate > 200) {
    lastDisplayUpdate = millis();
    display.clearDisplay();

    if (!isUnlocked) {
      // 1. HIGHEST PRIORITY: Hide everything behind the lock screen
      display.setCursor(0, 10);
      display.println("System Locked");
      display.setCursor(0, 30);
      display.println("Enter Owner PIN:");
      
      String maskedPIN = "";
      for (unsigned int i = 0; i < currentInput.length(); i++) maskedPIN += "*";
      display.setCursor(0, 50);
      display.println(maskedPIN);
    }
    else if (isHardLocked) {
      // 2. SECOND PRIORITY: Show escalated hardware fault
      display.setCursor(0, 0);
      display.println("!! HARDWARE FAULT !!");
      display.setCursor(0, 16);
      display.println(faultReason);

      if (dangerPresent) {
        display.setCursor(0, 36);
        display.println("DANGER STILL PRESENT");
        display.setCursor(0, 46);
        display.println("Please fix hardware.");
      } else {
        display.setCursor(0, 36);
        display.println("Hardware Safe.");
        display.setCursor(0, 46);
        display.println("Press # to Reset Relay");
      }
    }
    else if (isFaultTriggered && !isHardLocked) {
      // 3. THIRD PRIORITY: Show auto-recovering grace period
      display.setCursor(0, 10);
      display.println("! ANOMALY DETECTED !");
      display.setCursor(0, 30);
      display.println(faultReason);
      display.setCursor(0, 50);
      display.println("Auto-Recovering...");
    }
    else {
      // 4. LOWEST PRIORITY: Normal Telemetry Dashboard
      display.setCursor(0, 0);
      display.println("--- PV MONITORING ---");
      
      display.setCursor(0, 20);
      display.print("Voltage: "); display.print(currentVoltage); display.println(" V");

      display.setCursor(0, 26);
      display.print("Current: "); display.print(currentAmps); display.println(" A");
      
      display.setCursor(0, 36);
      display.print("Temp:    "); display.print(panelTemp); display.println(" C");

      display.setCursor(0, 50);
      display.println("Press * to Lock");
    }
    
    display.display();
  }
}

// Custom function to write to the SD card
// --- LOGGING UTILITIES ---

void logAccess(bool wasSuccessful) {
  File logFile = SD.open("access.txt", FILE_WRITE);
  String logEntry = "Time(ms):" + String(millis()) + ", Auth_Status: " + (wasSuccessful ? "GRANTED" : "DENIED");
  
  if (logFile) {
    logFile.println(logEntry);
    logFile.close(); 
  }
  Serial.println("[ACCESS LOG] " + logEntry);
}

void logTelemetry(float v, float i, float t) {
  File dataFile = SD.open("data.txt", FILE_WRITE);
  String dataEntry = "Time:" + String(millis()) + "ms, V:" + String(v) + ", I:" + String(i) + ", T:" + String(t);
  
  if (dataFile) {
    dataFile.println(dataEntry);
    dataFile.close();
  }
  Serial.println("[DATA LOG] " + dataEntry);
}

void logEvent(String eventType, String details) {
  File eventFile = SD.open("events.txt", FILE_WRITE);
  String eventEntry = "Time:" + String(millis()) + "ms [" + eventType + "] " + details;
  
  if (eventFile) {
    eventFile.println(eventEntry);
    eventFile.close();
  }
  Serial.println("[EVENT LOG] " + eventEntry); // Prints directly to the Wokwi terminal
}

// Custom function to handle OLED screen updates cleanly without repeating code
void updateScreen(String line1, String line2) {
  display.clearDisplay();
  display.setCursor(0, 10);
  display.println(line1);
  display.setCursor(0, 30);
  display.println(line2);
  display.display();
}