#ifndef CLAIRE_H
#define CLAIRE_H

#include <Arduino.h>
#include <EEPROM.h>

#define VERSION "0.1.4"

#define SENSOR_SAMPLE_SIZE 5
#define SENSOR_MIN_VALUE 0
#define SENSOR_MAX_VALUE 1023
#define SENSOR_OUTLIER_THRESHOLD 10


struct Output {
  Output(const Output&) = delete;
  Output(int pin, String name, int tube, bool inflow) {
    this->pin = pin;
    this->name = name;
    this->tube = tube;
    this->inflow = inflow;
  }
  int pin = -1;
  String name = "unknown pump";
  int duty = 0;
  int tube = -1;
  int inflow = false;
};

struct Sensor {
  Sensor(int pin, String name, int tube) {
    this->pin = pin;
    this->name = name;
    this->tube = tube;
  }
  int pin = -1;
  String name = "unknown sensor";
  int tube = -1;
};

// default pumps (uses double memory as header imported twice)
namespace default_pump_defs {
static Output TUBE0_IN { 2, "Tube0_inflow", 0, true };
static Output TUBE0_OUT { 3, "Tube0_outflow", 0, false };
static Output TUBE1_IN { 7, "Tube1_inflow", 1, true }; //fixme: chan. 3 on driver-board seems wonky, switched to unused chan.
static Output TUBE1_OUT { 5, "Tube1_outflow", 1, false };
static Output STREAM_OUT { 6, "Stream_outflow", -1, false };
}

static Output** default_pumps = new Output*[5] {
  &default_pump_defs::TUBE0_IN,
  &default_pump_defs::TUBE0_OUT,
  &default_pump_defs::TUBE1_IN,
  &default_pump_defs::TUBE1_OUT,
  &default_pump_defs::STREAM_OUT
};


namespace default_sensor_defs {
static const auto TUBE0_HEIGHT = Sensor{ 10, "Tube0_height", 0 };
static const auto TUBE1_HEIGHT = Sensor{ 11, "Tube1_height", 1 };
}

static Sensor default_sensors[2] = {
  default_sensor_defs::TUBE0_HEIGHT,
  default_sensor_defs::TUBE1_HEIGHT
};

class Claire {
public:
  Claire() = default;
  Claire(Output *pumpDefinitions, int size);
    
  bool DEBUG = true;
  bool VERBOSE = true;
  bool begin();
  // Takes pump to actuate and the duty in percentage [0..100].
  // Expect lower PWM frequency as duty decreases to cope with stalling of pump;
  // e.g. duty of 5% might result in 5 seconds on and 95 seconds off.
  bool setPump(Output &output, int duty);
  int getRange(const Sensor &sensor);
  void loadEEPROMCalibration();
  void defineNewPumps(Output *newPumps, int sizeNew);
  Output **pumps = default_pumps;
  int pumpCount = sizeof(default_pumps) / sizeof(default_pumps[0]);
  Sensor *sensors = &default_sensors[0];
  int sensorCount = 2;
private:
  bool check();
};

#endif
