#include "Claire.h"

// use default definitions of pumps and sensors of reference physical implementation
using namespace default_pump_defs;
using namespace default_sensor_defs;


Claire claire = Claire();
void setup() {
  // set debug to be verbose in output
  claire.DEBUG = true;

  // setup 9600 baud for serial connection
  Serial.begin(9600);

  claire.begin();
}

void loop() {
  Serial.println(claire.getRange(TUBE0_HEIGHT));
  int ok = claire.setPump(TUBE0_IN, getDuty());
  Serial.println(claire.getRange(TUBE0_HEIGHT));
  ok = claire.setPump(TUBE0_OUT, getDuty());
  digitalWrite(LED_BUILTIN, !ok);
  bool f = true;
  f &= false;
}

// read a duty from serial to actuate on demonstrator
int getDuty() {
  // command read from serial
  int cmd = -1;

  // waiting for input
  Serial.println("> ");
  while (Serial.available() == 0) {}
  cmd = Serial.parseInt();

  while (Serial.available() > 0) {  // Flush the buffer
    Serial.read();
  }

  while (cmd < 0 || cmd > 255) {
    Serial.println("Out of bounds [0..255]: " + String(cmd));
    while (Serial.available() == 0) {}
    cmd = Serial.parseInt();
    while (Serial.available() > 0) {  // Flush the buffer
      Serial.read();
    }
  }
  return cmd;
}
