#pragma once
#include <Arduino.h>

class SecurityAuthenticator {
  private:
    String storedHash; 

    String generateHash(String input) {
      unsigned long hash = 5381;
      // Fixed: Changed 'int i' to 'unsigned int i' to clear the compiler warning
      for (unsigned int i = 0; i < input.length(); i++) {
        hash = ((hash << 5) + hash) + input[i]; 
      }
      return String(hash);
    }

  public:
    SecurityAuthenticator() { 
      storedHash = ""; 
    }
    
    void loadSavedHash(String hash) { 
      storedHash = hash; 
    }

    String setupNewPIN(String newPIN) {
      storedHash = generateHash(newPIN);
      return storedHash;
    }

    bool validatePIN(String inputPIN) { 
      return generateHash(inputPIN) == storedHash; 
    }
};