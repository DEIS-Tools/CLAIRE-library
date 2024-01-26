#ifndef CLAIRE_MGMT_H
#define CLAIRE_MGMT_H

#include <Arduino.h>
#include <EEPROM.h>

enum class Container : int { TUBE_0 = 0,
                             TUBE_1 = 1,
                             STREAM_0 = 2 };


enum class Pump : int { INFLOW = 0,
                        OUTFLOW = 1 };

class Claire {
public:
  bool DEBUG = true;
  static const int containerCount = 3;
  static const int pumpCount = 2;
  // todo (load calibration data, if any)
  bool begin();
  void assignPumpPin(Container container, Pump pump, int pin);
  // Takes pump to actuate and the duty in percentage [0..100].
  // Expect lower PWM frequency as duty decreases to cope with stalling of pump;
  // e.g. duty of 5% might result in 5 seconds on and 95 seconds off.
  bool setPump(Container container, Pump pump, int duty);
  int getPumpPin(Container container, Pump pump);
  void loadEEPROMCalibration();

private:
  int _pumpPins[containerCount][pumpCount] = { { 3, -1 }, { -1, -1 }, { -1, -1 } };
  bool _enabledPins[containerCount][pumpCount];
};

#endif
