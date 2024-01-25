#include "claire.h"

bool DEBUG = true;

Claire sys = Claire();

void setup() {
  // put your setup code here, to run once:
  pinMode(3, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(9600);
}

void pump(int duty) {
  /*
    pump will not run without power-cycling below pwm signal to IRFZ44N of about 30 [0..255]
  */

  if (DEBUG) {
    analogWrite(3, duty);
    return;
  }


  if (duty < 15) {
    // turn off pump

    analogWrite(3, 0);
    Serial.println("set duty to 0");

  } else if (15 <= duty && duty < 30) {
    // do wider PWM frequency if pump stalls (TODO: test with pump-height of ~70cm)

    // ignore lower wished duty, and set
    analogWrite(3, 30);
    Serial.println("set duty to 30");
  } else {
    analogWrite(3, duty);
    Serial.println("set duty to " + String(duty));
  }

}

int getDuty() {
  // command read from serial
  int cmd = -1;

  // waiting for input
  Serial.println("Input duty: ");
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

  Serial.print("read: ");
  Serial.println(cmd);
  return cmd;
}




void loop() {
  // put your main code here, to run repeatedly:
  //pump(getDuty());
  sys.setPump(Container::TUBE_0, Pump::INFLOW, getDuty());
}
