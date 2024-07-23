#ifndef CLAIRE_H
#define CLAIRE_H

#include <Arduino.h>
#include <EEPROM.h>

#define VERSION "0.1.11"

#define OUTPUT_GPIO_MIN 2
#define OUTPUT_GPIO_MAX 7

#define SENSOR_SAMPLE_SIZE 10
#define SENSOR_MIN_VALUE 0
#define SENSOR_MAX_VALUE 1023
#define SENSOR_OUTLIER_THRESHOLD 10
#define SENSOR_NUM_ERROR_BAIL 2
#define SENSOR_WATER_SETTLE_TIMEOUT 500
#define SENSOR_BACKOFF 250
#define SENSOR_MAX_TRIES 3

#define TUBE_MAX_LEVEL 900

// scaling factor on error in unit mm
#define SET_LEVEL_PROPORTIONAL_GAIN 50
#define SET_LEVEL_INTEGRAL_GAIN 5
#define SET_LEVEL_ADD_MIN_ACTUATE_TIME 500
#define SET_LEVEL_ADD_MAX_ACTUATE_TIME 5000
#define SET_LEVEL_SUB_MIN_ACTUATE_TIME 1000
#define SET_LEVEL_SUB_MAX_ACTUATE_TIME 5000
// allowed inaccuracy of ranging when setting level
#define SET_LEVEL_HYSTERESIS 5

struct Output {
  Output(const Output&) = delete;
  Output(int pin, String name, int tube, bool inflow, int solenoid_pin) {
    this->pin = pin;
    this->name = name;
    this->tube = tube;
    this->inflow = inflow;
    this->solenoid_pin = solenoid_pin;
  }
  int pin = -1;
  String name = "unknown pump";
  int duty = 0;
  int tube = -1;
  int inflow = false;
  int solenoid_pin = -1;
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
static Output TUBE0_IN { 2, "Tube0_inflow", 0, true, -1 };
static Output TUBE0_OUT { 3, "Tube0_outflow", 0, false, 8 };
static Output TUBE1_IN { 4, "Tube1_inflow", 1, true, -1 }; 
static Output TUBE1_OUT { 5, "Tube1_outflow", 1, false, 9 };
static Output STREAM_IN { 6, "Stream_inflow", -1, false, -1 };
static Output STREAM_OUT { 7, "Stream_outflow", -1, false, -1 };
}

const int PUMP_COUNT = 6;
static Output** default_pumps = new Output*[PUMP_COUNT + 1] {
  &default_pump_defs::TUBE0_IN,
  &default_pump_defs::TUBE0_OUT,
  &default_pump_defs::TUBE1_IN,
  &default_pump_defs::TUBE1_OUT,
  &default_pump_defs::STREAM_IN,
  &default_pump_defs::STREAM_OUT,
  nullptr
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
  bool ENABLE_RANGE_CONFLICT = false; 
  bool begin();
  bool setPump(Output &output, int duty);
  float getRange(const Sensor &sensor, bool filtered = true);
  void loadEEPROMCalibration(); 
  void saveEEPROMCalibration(); 
  void defineNewPumps(Output *newPumps, int sizeNew);
  void eStop();
  void testOutput();
  bool setLevel(Output &in, Output &out, int level);
  Output **pumps = default_pumps;
  int pumpCount = PUMP_COUNT; 
  Sensor *sensors = &default_sensors[0];
  int sensorCount = SENSOR_COUNT;
private:
  void getRangeImpl(const Sensor &sensor, bool filtered = true);
  bool check();
  SensorReading sensorReadingTemp;
};

#endif
