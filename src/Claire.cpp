#include "Claire.h"
#include "math.h"
#include "AceSorting.h"

using namespace default_pump_defs;

Claire::Claire(Output *pumpDefinitions, int size) {
  defineNewPumps(pumpDefinitions, size);
  begin();
}

// Initialise system with default pump configuration
bool Claire::begin() {
  Serial.println(String("Initialising CLAIRE water management v") + VERSION);

  pinMode(LED_BUILTIN, OUTPUT);
  int ok = true;
  digitalWrite(LED_BUILTIN, !ok);
  // check that none of the assigned pins are of sentinel value nor share a pin
  for (int i = 0; i < pumpCount; i++) {
    if (pumps[i]->pin == -1) {
      if (ok) {
        // on first error, print legend FIXME
        Serial.println("Indicies. Containers: 0: Tube1, 1: Tube2, 2: Stream-res. Pumps 0: Inflow, 1: outflow.");
      }   
      Serial.println("ERROR: Pump PWM pin unset for resource: " + String(pumps[i]->name) + " pin: " + String(pumps[i]->pin));
      ok = false;
    } else {
      // set given pin as output
      pinMode(pumps[i]->pin, OUTPUT);
      // ensure this pump is off on init
      digitalWrite(pumps[i]->pin, 0);

      // similarly for solenoid pins
      if (pumps[i]->solenoid_pin != -1) {
        pinMode(pumps[i]->solenoid_pin, OUTPUT);
        digitalWrite(pumps[i]->solenoid_pin, 0);
      }
    } 

    digitalWrite(LED_BUILTIN, !ok);
  }

  for (int i = 0; i < sensorCount; i++) {
    if (sensors[i].pin == -1) {
      Serial.println("ERROR: Sensor PWM pin unset for resource: " + String(sensors[i].name) + " pin: " + String(sensors[i].pin));
    } else {
      pinMode(sensors[i].pin, INPUT_PULLUP);
    }

  }
  
  // todo: fixup def and binding of solenoids to pumps
  pinMode(8, OUTPUT);
  pinMode(9, OUTPUT);
  digitalWrite(8, LOW);
  digitalWrite(9, LOW);
  
  return ok;
}

bool Claire::check() {
  // todo: do check on pins (no sentinel, no duplicate, no OOB for given platform)
  return true;
}

void Claire::defineNewPumps(Output *newPumps, int sizeNew) {
  // disable pinMode on current pumps
  for (int i = 0; i < pumpCount; i++) {
    pinMode(pumps[i]->pin, INPUT);
  }
  
  //todo: fixme
  // set new pumps
  //pumps = newPumps;
  //pumpCount = sizeNew;

  int ok = check();

  // enable pinMode for new pumps
  for (int i = 0; i < pumpCount; i++) {
    pinMode(pumps[i]->pin, OUTPUT);
  }
}


float filter_samples(int readings[], int sample_count, bool DEBUG, bool VERBOSE) {
  int filteredValues[sample_count];
  float raw_mean = 0; 
  float filtered_avg = 0;
  int j = 0;

  // Sanity check
  if (sample_count <= 0) {
    Serial.println("No data points to filter.");
    return NAN;
  }

  // Special case if sample count 1 one
  if (sample_count == 1) {
    return readings[0];
  }

  if (DEBUG) {
    for (int i = 0; i < sample_count; i++) {
      Serial.println("Raw reading " + String(i) + ": " + String(readings[i]));
    }
  }
  ace_sorting::insertionSort(readings, sample_count);

  if (sample_count%2) {
    // Even number of samples
    raw_mean = (readings[sample_count/2] + readings[(sample_count/2) - 1])/2;
  } else {
    // Odd number of samples
    raw_mean = readings[sample_count/2];
  }

  for (int i = 0; i < sample_count; i++) {
    // Discard outliers
    if (abs(readings[i] - raw_mean) < SENSOR_OUTLIER_THRESHOLD) {
      filteredValues[j] = readings[i];
      j++;
    }
  }

  if (j == 0) {
    Serial.println("All data points were classified as outliers.");
    return NAN;
  }
  
  for (int i = 0; i < j; i++) {
    filtered_avg += filteredValues[i];
  }
  filtered_avg =  filtered_avg / j;

  // fixme: add debug from local Claire object scope
  if (j < sample_count) {
    Serial.println("Outliers detected (raw mean) (filtered average) " + String(raw_mean) + " : " + String(filtered_avg));
  }

  return filtered_avg; 
}

void Claire::getRangeImpl(const Sensor &sensor, bool filtered) {
  // if duty for inflow, for given tube is positive; remember duty, turn off for epsilon, then reset duty
  int conflicting_pump_old_duty = -1;
  Output *conflicting_pump;
  sensorReadingTemp.failure = false;

  if (ENABLE_RANGE_CONFLICT && DEBUG && VERBOSE) {
    Serial.println("Checking for pump conflicting with ranging");
  }
  
  for (int i = 0; i < pumpCount; i++) {
    if (DEBUG && VERBOSE) {
      Serial.println("\t" + String(pumps[i]->name) + ", pin: " + String(pumps[i]->pin) + ", inflow: " + String(pumps[i]->inflow) + ", duty:" + String(pumps[i]->duty));
    }
    if (pumps[i]->tube == sensor.tube && pumps[i]->inflow && pumps[i]->duty > 0) {
      if (DEBUG && VERBOSE) {
        Serial.println("Stopping " + String(pumps[i]->name) + " while ranging");
      }

      if (ENABLE_RANGE_CONFLICT) {
        // remember pump and set duty before stopping
        conflicting_pump_old_duty = pumps[i]->duty;
        conflicting_pump = pumps[i];
        setPump(*conflicting_pump, 0);
        break;
      }
    }
  }
  if (ENABLE_RANGE_CONFLICT && conflicting_pump_old_duty == -1 && DEBUG && VERBOSE) {
    Serial.println("No ranging conflict found");
  }
  
  // delay shortly to settle water in tube before sampling
  if (ENABLE_RANGE_CONFLICT) delay(SENSOR_WATER_SETTLE_TIMEOUT);

  // do sampling, only once if unfiltered
  int sample_count = filtered ? SENSOR_SAMPLE_SIZE : 1;
  for (int i = 0; i < sample_count ; i++) {
    sensorReadingTemp.samples[i] = pulseIn(sensor.pin, HIGH);
    if (sensorReadingTemp.samples[i] == 0 || sensorReadingTemp.samples[i] > 1000) {
      sensorReadingTemp.failure = true;
    }
  }

  float res = -1;
  if (!sensorReadingTemp.failure) {
    res = filter_samples(sensorReadingTemp.samples, sample_count, DEBUG, VERBOSE);
  }

  if (DEBUG && sensorReadingTemp.failure) {
    Serial.print(sensor.name + " raw ranging:");
    for (auto elem : sensorReadingTemp.samples) {
      Serial.print(String(elem) + ", ");
    }
    Serial.println("filtered: " + String(res));
  }

  if (ENABLE_RANGE_CONFLICT && conflicting_pump_old_duty != -1) {
    setPump(*conflicting_pump, conflicting_pump_old_duty);
    if (DEBUG && VERBOSE) {
        Serial.println("Started " + String(conflicting_pump->name) + " after ranging");
      }
  }

  // set result
  sensorReadingTemp.res = res;
}

float Claire::getRange(const Sensor &sensor, bool filtered = true) {
  int error_count = 0;
  do {
    Claire::getRangeImpl(sensor, filtered);
    if (sensorReadingTemp.res == -1) {
      error_count += 1;
      if (DEBUG) {
        Serial.println("ERROR ranging " + String(sensor.name) + ", "+ String(error_count) + " time(s)");
      }
    }
  } while(sensorReadingTemp.failure && error_count < SENSOR_MAX_TRIES - 1);
  
  if (VERBOSE && (sensorReadingTemp.res == -1 || isnan(sensorReadingTemp.res))) {
    Serial.println("ERROR: Could not acquire signal from " + String(sensor.name));
  }
  
  return sensorReadingTemp.res;
}


// Takes pump to actuate and the duty in percentage [0..100].
// Expect lower PWM frequency as duty decreases to cope with stalling of pump;
// e.g. duty of 5% might result in 5 seconds on and 95 seconds off.
bool Claire::setPump(Output &output, int duty) {
  // validate input
  if (duty < 0 || duty > 100) {
    Serial.println("ERROR: Duty out of bounds at: " + String(duty) + " must be [0..100]");
    return false;
  }

  // map percentage to byte
  int duty_byte = map(abs(duty), 0, 100, 0, 255);


  // write PWM signal to pin
  analogWrite(output.pin, duty_byte);

  // set written duty for later start/stop during ranging
  output.duty = duty;

  // check for solenoid to handle 
  if (output.solenoid_pin != -1) {
    if (duty > 0) {
      if (DEBUG) Serial.println("Setting high\t solenoid_pin: " + String(output.solenoid_pin));
      digitalWrite(output.solenoid_pin, HIGH);
    } else {
      if (DEBUG) Serial.println("Setting low\t solenoid_pin: " + String(output.solenoid_pin));
      digitalWrite(output.solenoid_pin, LOW);
    }
  }

  if (DEBUG) {
    Serial.println("Set " + output.name + " to " + String(duty) + "%");
  }

  return true;
}

void Claire::eStop() {
  for (Output** p = pumps; *p != nullptr; ++p) {
    setPump(**p, 0);
  }
}

bool Claire::setLevel(Output &in, Output &out, int level) {
  if (0 > level || level > TUBE_MAX_LEVEL) {
    Serial.println("WARN: Level (" + String(level) + ") is out of bounds [0.." + String(TUBE_MAX_LEVEL) + "]");
    return false;
  }

  Sensor sensor = default_sensor_defs::TUBE1_HEIGHT;

  if (in.tube == 1) {
    sensor = default_sensor_defs::TUBE1_HEIGHT; 
  } else if (in.tube == 2) {
    sensor = default_sensor_defs::TUBE2_HEIGHT;
  } else {
    Serial.println("ERROR: Unknown tube referenced, aborting setLevel");
    return false;
  }

  int curr = getRange(sensor);
  int diff = level - curr;
  // Accummulative difference/error, only takes effect if diff < 100 as that is the cut-off range for the P-part with max actuation time.
  int acc_diff = abs(diff) > 50 ? 0 : diff; 
  bool goal = curr == level;
  int close_error_count = 0;

  while (!goal && close_error_count < 3) {
    // PI-controller
    int output = SET_LEVEL_PROPORTIONAL_GAIN * diff + SET_LEVEL_INTEGRAL_GAIN * acc_diff;
    if (diff < 0) {
      int delay_ms = max(min(abs(output), SET_LEVEL_ADD_MAX_ACTUATE_TIME), SET_LEVEL_ADD_MIN_ACTUATE_TIME);
      // adding water
      if (VERBOSE) Serial.println("D: " + String(diff) + " (C: " + String(curr) + " L: " + String(level) + ") Adding water for: " + String(delay_ms) + " ms"); 
      setPump(in, 20);
      delay(delay_ms);
      setPump(in, 0);
    } else {
      int delay_ms = max(min(abs(output), SET_LEVEL_SUB_MAX_ACTUATE_TIME), SET_LEVEL_SUB_MIN_ACTUATE_TIME);
      // subtracting water
      if (VERBOSE) Serial.println("D: " + String(diff) + " (C: " + String(curr) + " L: " + String(level) + ") Removing water for: " + String(delay_ms) + " ms"); 
      setPump(out, 20);
      delay(delay_ms);
      setPump(out, 0);
      delay(100); // To prevent some inteference with the next water level reading. Only happens with outflow.
    }

    //delay to settle
    if (ENABLE_RANGE_CONFLICT) delay(SENSOR_WATER_SETTLE_TIMEOUT);

    // check diff
    curr = getRange(sensor);
    int tries = 1;
    while (curr == -1) {
      curr = getRange(sensor);
      tries++;
      if (tries > 2) {
        return false;
      }
    }
    diff = level - curr;
    acc_diff = abs(diff) > 50 ? 0 : acc_diff + diff;
    goal = curr == level;
    
    if (abs(diff) < SET_LEVEL_HYSTERESIS) {
      close_error_count++;
    } else {
      close_error_count = max(close_error_count - 1, 0);
    }
  }

  if (close_error_count >= 3) {
    Serial.println("Level set with a error: " + String(diff));
  } else {
    Serial.println("Level set with no error");
  }

  if (abs(diff) > SET_LEVEL_HYSTERESIS) {
    return false;
  } else {
    return true;
  }
}

void Claire::testOutput() {
  Serial.println("WARN: Will run until power cycled. Sweeping all channel PWM signals with solenoid activation");
  
  int duty = 0;
  int incr = -10;
  while (1) {
    Serial.print("Duty: " + String(duty) + " ");
    for (Output** p = pumps; *p != nullptr; ++p) {
      Serial.print("pin: " + String((*p)->pin) + " (" + String((*p)->solenoid_pin) + "), ");
    setPump(**p, duty);
    }

    // flip incr sign if hitting bound in this iter
    if (duty + incr < 0) {
      incr = 10;
    } else if (duty + incr > 100) {
      incr = -10; 
    }
    duty += incr;

    delay(250);
    Serial.println();
  }
}

void Claire::loadEEPROMCalibration() {
  // eeprom test
  Serial.println("EEPROM: " + String(EEPROM.read(0)));
  EEPROM.write(0, 42);
  Serial.println("EEPROM: " + String(EEPROM.read(0)));
  EEPROM.write(0, 255);
}

void Claire::saveEEPROMCalibration() {

}
