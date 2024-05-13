#ifndef CLAIRE_H
#define CLAIRE_H

#include <Arduino.h>
#include <EEPROM.h>

#define VERSION "0.1.3"

#define SENSOR_SAMPLE_SIZE 10
#define SENSOR_MIN_VALUE 0
#define SENSOR_MAX_VALUE 1023
#define SENSOR_OUTLIER_THRESHOLD 10



struct Output {
  Output(int pin, String name) {
    this->pin = pin;
    this->name = name;
  }
  int pin = -1;
  String name = "unknown pump";
};

struct Sensor {
  Sensor(int pin, String name) {
    this->pin = pin;
    this->name = name;
  }
  int pin = -1;
  String name = "unknown sensor";
};

// default pumps (uses double memory as header imported twice)
namespace default_pump_defs {
static const auto TUBE0_IN = Output{ 2, "Tube0_inflow" };
static const auto TUBE0_OUT = Output{ 3, "Tube0_outflow" };
static const auto TUBE1_IN = Output{ 4, "Tube1_inflow" };
static const auto TUBE1_OUT = Output{ 5, "Tube1_outflow" };
static const auto STREAM_OUT = Output{ 6, "Stream_outflow" };
}

static Output default_pumps[5] = {
  default_pump_defs::TUBE0_IN,
  default_pump_defs::TUBE0_OUT,
  default_pump_defs::TUBE1_IN,
  default_pump_defs::TUBE1_OUT,
  default_pump_defs::STREAM_OUT
};


namespace default_sensor_defs {
static const auto TUBE0_HEIGHT = Sensor{ 10, "Tube0_height" };
static const auto TUBE1_HEIGHT = Sensor{ 11, "Tube1_height" };
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
  bool begin();
  // Takes pump to actuate and the duty in percentage [0..100].
  // Expect lower PWM frequency as duty decreases to cope with stalling of pump;
  // e.g. duty of 5% might result in 5 seconds on and 95 seconds off.
  bool setPump(const Output &output, int duty);
  int getRange(const Sensor &sensor);
  void loadEEPROMCalibration();
  void defineNewPumps(Output *newPumps, int sizeNew);
  Output *pumps = &default_pumps[0];
  int pumpCount = sizeof(default_pumps) / sizeof(default_pumps[0]);
  Sensor *sensors = &default_sensors[0];
  int sensorCount = 2;
private:
  bool check();
};

#endif
