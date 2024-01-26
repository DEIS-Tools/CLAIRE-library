#include "claire.h"

Claire claire = Claire();

void setup() {
  // set debug
  claire.DEBUG = true;

  Serial.begin(9600);

  claire.begin();  
}

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

void loop() {
  claire.setPump(Container::TUBE_0, Pump::INFLOW, getDuty());
}
