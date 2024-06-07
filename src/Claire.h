#ifndef CLAIRE_H
#define CLAIRE_H

#include <Arduino.h>
#include <EEPROM.h>

#define VERSION "0.1.5"

#define SENSOR_SAMPLE_SIZE 5
#define SENSOR_MIN_VALUE 0
#define SENSOR_MAX_VALUE 1023
#define SENSOR_OUTLIER_THRESHOLD 10
#define SENSOR_NUM_ERROR_BAIL 2
#define SENSOR_WATER_SETTLE_TIMEOUT 100
#define SENSOR_BACKOFF 250
#define SENSOR_MAX_TRIES 3

#define TUBE_MAX_LEVEL 700

// scaling factor on error in unit mm
#define SET_LEVEL_SCALING_FACTOR 50
// allowed inaccuracy of ranging when setting level
#define SET_LEVEL_HYSTERESIS 5

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

//class Claire;
class SensorReading {
  private:
    friend class Claire;
    float res = -1;
    int samples[SENSOR_SAMPLE_SIZE] = { -1 };
    bool failure = false;
  public:
};


// default pumps (uses double memory as header imported twice)
namespace default_pump_defs {
static Output TUBE0_IN { 2, "Tube0_inflow", 0, true };
static Output TUBE0_OUT { 3, "Tube0_outflow", 0, false };
static Output TUBE1_IN { 6, "Tube1_inflow", 1, true }; //fixme: chan. 3 on driver-board seems wonky, switched to unused chan.
static Output TUBE1_OUT { 5, "Tube1_outflow", 1, false };
static Output STREAM_OUT { 7, "Stream_outflow", -1, false };
}

const int PUMP_COUNT = 5;
static Output** default_pumps = new Output*[PUMP_COUNT] {
  &default_pump_defs::TUBE0_IN,
  &default_pump_defs::TUBE0_OUT,
  &default_pump_defs::TUBE1_IN,
  &default_pump_defs::TUBE1_OUT,
  &default_pump_defs::STREAM_OUT
};


namespace default_sensor_defs {
static const auto TUBE0_HEIGHT = Sensor{ 10, "Tube0_water_mm", 0 };
static const auto TUBE1_HEIGHT = Sensor{ 11, "Tube1_water_mm", 1 };
}

const int SENSOR_COUNT = 2;
static Sensor default_sensors[SENSOR_COUNT] = {
  default_sensor_defs::TUBE0_HEIGHT,
  default_sensor_defs::TUBE1_HEIGHT
};

class Claire {
public:
  Claire() = default;
  Claire(Output *pumpDefinitions, int size);
    
  bool DEBUG = false;
  bool VERBOSE = false;
  bool begin();
  // Takes pump to actuate and the duty in percentage [0..100].
  // Expect lower PWM frequency as duty decreases to cope with stalling of pump;
  // e.g. duty of 5% might result in 5 seconds on and 95 seconds off.
  bool setPump(Output &output, int duty);
  float getRange(const Sensor &sensor);
  void loadEEPROMCalibration();
  void defineNewPumps(Output *newPumps, int sizeNew);
  void eStop();
  bool setLevel(Output &in, Output &out, int level);
  Output **pumps = default_pumps;
  int pumpCount = PUMP_COUNT; 
  Sensor *sensors = &default_sensors[0];
  int sensorCount = SENSOR_COUNT;
private:
  void getRangeImpl(const Sensor &sensor);
  bool check();
  SensorReading sensorReadingTemp;
};

#endif
