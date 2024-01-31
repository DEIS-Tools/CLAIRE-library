#include "claire.h"

using namespace default_pump_defs;

/*
static Output custom_def[5] = {
  default_pump_defs::TUBE0_IN,
  default_pump_defs::TUBE0_OUT,
  default_pump_defs::TUBE1_IN,
  default_pump_defs::TUBE1_OUT,
  default_pump_defs::STREAM_OUT
};
int custom_def_size = 5;

Claire claire = Claire(custom_def, custom_def_size);
*/

Claire claire = Claire();
void setup() {
  // set debug
  claire.DEBUG = true;

  // setup 9600 baud
  Serial.begin(9600);




  claire.begin();
}

void loop() {
  int ok = claire.setPump(TUBE0_IN, getDuty());
  digitalWrite(LED_BUILTIN, !ok);
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
