#ifndef CLAIRE_H
#define CLAIRE_H

#include <Arduino.h>
#include <EEPROM.h>

struct Output {
  Output(int pin, String name) {
    this->pin = pin;
    this->name = name;
  }
  int pin = -1;
  String name = "unknown";
};

// default pumps (uses double memory as header imported twice)
namespace default_pump_defs {
static const auto TUBE0_IN = Output{ 3, "Tube0_inflow" };
static const auto TUBE0_OUT = Output{ 5, "Tube0_outflow" };
static const auto TUBE1_IN = Output{ 6, "Tube1_inflow" };
static const auto TUBE1_OUT = Output{ 9, "Tube1_outflow" };
static const auto STREAM_OUT = Output{ 10, "Stream_outflow" };
}


static Output default_pumps[5] = {
  default_pump_defs::TUBE0_IN,
  default_pump_defs::TUBE0_OUT,
  default_pump_defs::TUBE1_IN,
  default_pump_defs::TUBE1_OUT,
  default_pump_defs::STREAM_OUT
};

class Claire {
public:
  Claire() = default;
  Claire(Output *pumpDefinitions, int size);
    
  bool DEBUG = true;
  bool begin();
  // Takes pump to actuate and the duty in percentage [0..100].
  // Expect lower PWM frequency as duty decreases to cope with stalling of pump;
  // e.g. duty of 5% might result in 5 seconds on and 95 seconds off.
  bool setPump(const Output &output, int duty);
  void loadEEPROMCalibration();
  void defineNewPumps(Output *newPumps, int sizeNew);
  Output *pumps = &default_pumps[0];
  int pumpCount = sizeof(default_pumps) / sizeof(default_pumps[0]);
private:
  bool check();
};

#endif
