#include "claire.h"


// Takes pump to actuate and the duty in percentage [0..100].
// Expect lower PWM frequency as duty decreases to cope with stalling of pump;
// e.g. duty of 5% might result in 5 seconds on and 95 seconds off.
bool Claire::setPump(Container container, Pump pump, int duty) {
  // validate input
  if (duty < 0 || duty > 100) {
    Serial.println("Duty out of bounds at: " + String(duty) + " must be within [0..100]");
    return false;
  }

  // map percentage to byte
  int duty_byte = map(abs(duty), 0, 100, 0, 255);


  // lookup
  int pin = _pumpPins[int(container)][int(pump)];

  // write PWM signal
  analogWrite(pin, duty_byte);
}