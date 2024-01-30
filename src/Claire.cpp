#include "Claire.h"


Claire::Claire(Output *pumpDefinitions, int size) {
  defineNewPumps(pumpDefinitions, size);
  begin();
}

// Initialise system with default pump configuration
bool Claire::begin() {
  Serial.println("Initialising CLAIRE water management");

  pinMode(LED_BUILTIN, OUTPUT);
  int ok = true;
  digitalWrite(LED_BUILTIN, !ok);
  // check that none of the assigned pins are of sentinel value nor share a pin
  for (int i = 0; i < pumpCount; i++) {
    if (pumps[i].pin == -1) {
      if (ok) {
        // on first error, print legend FIXME
        Serial.println("Indicies. Containers: 0: Tube0, 1: Tube1, 2: Stream-res. Pumps 0: Inflow, 1: outflow.");
      }
      Serial.println("ERROR: Pump PWM pin unset for resource: " + String(pumps[i].name) + " pin: " + String(pumps[i].pin));
      ok = false;
    } else {
      // set given pin as output
      pinMode(pumps[i].pin, OUTPUT);
    }
    digitalWrite(LED_BUILTIN, !ok);
  }
  return ok;
}

bool Claire::check() {
  // do check on pins (no sentinel, no duplicate, no OOB for given platform)
  return true;
}

void Claire::defineNewPumps(Output *newPumps, int sizeNew) {
  // disable pinMode on current pumps
  for (int i = 0; i < pumpCount; i++) {
    pinMode(pumps[i].pin, INPUT);
  }

  // set new pumps
  pumps = newPumps;
  pumpCount = sizeNew;

  int ok = check();

  // enable pinMode for new pumps
  for (int i = 0; i < pumpCount; i++) {
    pinMode(pumps[i].pin, OUTPUT);
  }
}

// Takes pump to actuate and the duty in percentage [0..100].
// Expect lower PWM frequency as duty decreases to cope with stalling of pump;
// e.g. duty of 5% might result in 5 seconds on and 95 seconds off.
bool Claire::setPump(const Output &output, int duty) {
  // validate input
  if (duty < 0 || duty > 100) {
    Serial.println("Duty out of bounds at: " + String(duty) + " must be [0..100]");
    return false;
  }

  // map percentage to byte
  int duty_byte = map(abs(duty), 0, 100, 0, 255);


  // write PWM signal to pin
  analogWrite(output.pin, duty_byte);

  if (DEBUG) {
    Serial.println("Set " + output.name + " to " + String(duty) + "%");
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
