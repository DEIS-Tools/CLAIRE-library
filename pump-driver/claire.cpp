#include "claire.h"



// Initialise system
bool Claire::begin() {
  Serial.println("Initialising CLAIRE water management...");
  
  pinMode(LED_BUILTIN, OUTPUT);
  int ok = true;
  digitalWrite(LED_BUILTIN, !ok);
  // check that none of the assigned pins are of sentinel value
  for (int c = 0; c < Claire::containerCount; c++) {
    for (int p = 0; p < Claire::pumpCount; p++) {
      if (_pumpPins[c][p] == -1) {
        if (ok) {
          // on first error, print legend
          Serial.println("Indicies. Containers: 0: Tube0, 1: Tube1, 2: Stream-res. Pumps 0: Inflow, 1: outflow.");
        }
        Serial.println("ERROR: Pump PWM pin unset for resource: C" + String(c) + "_P" + String(p));
        ok = false;
      } else {
        // set given pin as output
        pinMode(_pumpPins[c][p], OUTPUT);
      }
      digitalWrite(LED_BUILTIN, !ok);
    }
  }
  return ok;
}

void Claire::assignPumpPin(Container container, Pump pump, int new_pin) {
  // unset pinMode if defined
  int prev_pin = _pumpPins[int(container)][int(pump)];
  
  if (prev_pin > 0) {
    // pin is valid, unset by making input
    pinMode(prev_pin, INPUT);
  } else {
    Serial.println("Previous pin: " + String(prev_pin) + " was not a valid");
  }
  
  // set given pin
  _pumpPins[int(container)][int(pump)] = new_pin;

  pinMode(new_pin, OUTPUT);
}

int Claire::getPumpPin(Container container, Pump pump) {
  // lookup DAC pin mapping
  return _pumpPins[int(container)][int(pump)];
}

// Takes pump to actuate and the duty in percentage [0..100].
// Expect lower PWM frequency as duty decreases to cope with stalling of pump;
// e.g. duty of 5% might result in 5 seconds on and 95 seconds off.
bool Claire::setPump(Container container, Pump pump, int duty) {
  // validate input
  if (duty < 0 || duty > 100) {
    Serial.println("Duty out of bounds at: " + String(duty) + " must be [0..100]");
    return false;
  }

  // map percentage to byte
  int duty_byte = map(abs(duty), 0, 100, 0, 255);

  int output_pin = Claire::getPumpPin(container, pump);

  // write PWM signal to pin
  analogWrite(output_pin, duty_byte);

  if (DEBUG) {
    Serial.println("Set pin: " + String(output_pin) + " to " + String(duty) + "%");
  }

  return true;
}

void Claire::loadEEPROMCalibration() {
    // eeprom test
  Serial.println("EEPROM: " + String(EEPROM.read(0)));
  EEPROM.write(0, 42);
  Serial.println("EEPROM: " + String(EEPROM.read(0)));
  EEPROM.write(0, 255);
}