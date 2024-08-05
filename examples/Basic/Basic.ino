#include "Claire.h"

// use default definitions of pumps and sensors of reference physical implementation
using namespace default_pump_defs;
using namespace default_sensor_defs;


Claire claire = Claire();
void setup() {
  // set debug to be verbose in output
  claire.DEBUG = true;
  claire.VERBOSE = false;

  // setup 9600 baud for serial connection
  Serial.begin(9600);

  claire.begin();
}

void loop() {
  Serial.println(claire.getRange(TUBE1_HEIGHT));
  Serial.println("Set duty TUBE0_IN:");
  int ok = claire.setPump(TUBE1_IN, getDuty());
  
  Serial.println(claire.getRange(TUBE1_HEIGHT));
  Serial.println("Set duty TUBE0_OUT:");
  ok = claire.setPump(TUBE1_OUT, getDuty());

  Serial.println(claire.getRange(TUBE2_HEIGHT));
  Serial.println("Set duty TUBE1_IN:");
  ok = claire.setPump(TUBE2_IN, getDuty());

  Serial.println(claire.getRange(TUBE2_HEIGHT));
  Serial.println("Set duty TUBE1_OUT:");
  ok = claire.setPump(TUBE2_OUT, getDuty());

  digitalWrite(LED_BUILTIN, !ok);
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
