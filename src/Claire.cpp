#include "Claire.h"
#include "math.h"

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
        Serial.println("Indicies. Containers: 0: Tube0, 1: Tube1, 2: Stream-res. Pumps 0: Inflow, 1: outflow.");
      }   
      Serial.println("ERROR: Pump PWM pin unset for resource: " + String(pumps[i]->name) + " pin: " + String(pumps[i]->pin));
      ok = false;
    } else {
      // set given pin as output
      pinMode(pumps[i]->pin, OUTPUT);
      // ensure this pump is off on init
      digitalWrite(pumps[i]->pin, 0);
    } 

    digitalWrite(LED_BUILTIN, !ok);
  }

  for (int i = 0; i < sensorCount; i++) {
    if (sensors[i].pin == -1) {
      Serial.println("ERROR: Sensor PWM pin unset for resource: " + String(sensors[i].name) + " pin: " + String(sensors[i].pin));
    } else {
      pinMode(sensors[i].pin, INPUT);
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


float filter_samples(int readings[]) {
  int filteredValues[SENSOR_SAMPLE_SIZE];
  float raw_avg = 0; 
  float filtered_avg = 0;
  int j = 0;

  for (int i = 0; i < SENSOR_SAMPLE_SIZE; i++) {
    raw_avg += readings[i];
  }
  raw_avg = raw_avg / SENSOR_SAMPLE_SIZE;

  for (int i = 0; i < SENSOR_SAMPLE_SIZE; i++) {
    // discard outliers
    if (abs(readings[i] - raw_avg) < SENSOR_OUTLIER_THRESHOLD) {
      filteredValues[j] = readings[i];
      j++;
    }
  }

  for (int i = 0; i < j; i++) {
    filtered_avg += filteredValues[i];
  }
  filtered_avg =  filtered_avg / j;


  // fixme: add debug from local Claire object scope
  if (true && raw_avg != filtered_avg) {
    Serial.println("Outliers detected (raw) (filtered) " + String(raw_avg) + " : " + String(filtered_avg));
  }

  return filtered_avg; 
}

void Claire::getRangeImpl(const Sensor &sensor) {
  // if duty for inflow, for given tube is positive; remember duty, turn off for epsilon, then reset duty
  int conflicting_pump_old_duty = -1;
  Output *conflicting_pump;

  if (DEBUG && VERBOSE) {
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

      // remember pump and set duty before stopping
      conflicting_pump_old_duty = pumps[i]->duty;
      conflicting_pump = pumps[i];
      setPump(*conflicting_pump, 0);
      break;
    }
  }
  if (conflicting_pump_old_duty == -1 && DEBUG && VERBOSE) {
    Serial.println("No conflict found");
  }

  // delay shortly to settle water in tube before sampling
  delay(SENSOR_WATER_SETTLE_TIMEOUT);

  // do sampling
  for (int i = 0; i < SENSOR_SAMPLE_SIZE; i++) {
    sensorReadingTemp.samples[i] = pulseIn(sensor.pin, HIGH);
    if (sensorReadingTemp.samples[i] == 0 || sensorReadingTemp.samples[i] > 1000) {
      sensorReadingTemp.failure = true;
    }
  }
  
  float res = -1;
  if (!sensorReadingTemp.failure) {
    res = filter_samples(sensorReadingTemp.samples);
  }

  if (DEBUG && sensorReadingTemp.failure) {
    Serial.print(sensor.name + " raw ranging:");
    for (auto elem : sensorReadingTemp.samples) {
      Serial.print(String(elem) + ", ");
    }
    Serial.println("filtered: " + String(res));
  }

  if (conflicting_pump_old_duty != -1) {
    setPump(*conflicting_pump, conflicting_pump_old_duty);
    if (DEBUG && VERBOSE) {
        Serial.println("Started " + String(conflicting_pump->name) + " after ranging");
      }

  }

  // set result
  sensorReadingTemp.res = res;
}

float Claire::getRange(const Sensor &sensor) {
  int error_count = 0;
  do {
    Claire::getRangeImpl(sensor);
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
    Serial.println("Duty out of bounds at: " + String(duty) + " must be [0..100]");
    return false;
  }

  // map percentage to byte
  int duty_byte = map(abs(duty), 0, 100, 0, 255);


  // write PWM signal to pin
  analogWrite(output.pin, duty_byte);

  // set written duty for later start/stop during ranging
  output.duty = duty;

  // if inflow, put respective solenoid high
  if (output.pin == 3 && duty > 0) {
    digitalWrite(8, HIGH);
  } else if (output.pin == 3 && duty == 0) {
    digitalWrite(8, LOW);
  }
  if (output.pin == 5 && duty > 0) {
    digitalWrite(9, HIGH);
  } else if (output.pin == 5 && duty == 0) {
    digitalWrite(9, LOW);
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
  // 
  if (0 > level || level > TUBE_MAX_LEVEL) {
    Serial.println("WARN: Level (" + String(level) + ") is out of bounds [0.." + String(TUBE_MAX_LEVEL) + "]");
    return false;
  }

  Sensor sensor = default_sensor_defs::TUBE0_HEIGHT;

  if (in.tube == 0) {
    sensor = default_sensor_defs::TUBE0_HEIGHT; 
  } else if (in.tube == 1) {
    sensor = default_sensor_defs::TUBE1_HEIGHT;
  } else {
    Serial.println("ERROR: Unknown tube referenced, aborting setLevel");
    return false;
  }

  int curr = getRange(sensor);
  int diff = level - curr;
  bool goal = curr == level;
  int close_error_count = 0;

  while (!goal && close_error_count < 3) {
    Serial.println("actuate"); 
    

    if (diff > 0) {
      // adding water
      setPump(in, 20);
      delay(SET_LEVEL_SCALING_FACTOR * diff);
      setPump(in, 0);
    } else {
      // subtracting water
      setPump(out, 20);
      delay(SET_LEVEL_SCALING_FACTOR * diff);
      setPump(out, 0);
    }

    // check diff
    curr = getRange(sensor);
    diff = level - curr;
    goal = curr == level;
    
    if (abs(diff) < SET_LEVEL_HYSTERESIS) {
      close_error_count++;
    } else {
      close_error_count--;
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


void Claire::loadEEPROMCalibration() {
  // eeprom test
  Serial.println("EEPROM: " + String(EEPROM.read(0)));
  EEPROM.write(0, 42);
  Serial.println("EEPROM: " + String(EEPROM.read(0)));
  EEPROM.write(0, 255);
}
