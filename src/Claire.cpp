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

int Claire::getRange(const Sensor &sensor) {
  int samples[SENSOR_SAMPLE_SIZE];

  // if duty for inflow, for given tube is positive; remember duty, turn off for epsilon, then reset duty
  
  int conflicting_pump_old_duty = -1;
  Output *conflicting_pump;

  if (DEBUG && VERBOSE) {
    Serial.println("Checking for pump conflicting with ranging");
  }
  for (int i = 0; i < pumpCount; i++) {
    if (DEBUG && VERBOSE) {
      Serial.print(String(pumps[i]->name) + ", pin: " + String(pumps[i]->pin) + ", inflow: " + String(pumps[i]->inflow) + ", duty:" + String(pumps[i]->duty));
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
  // delay shortly to settle water in tube before sampling
  delay(100);

  // sample 
  for (int i = 0; i < SENSOR_SAMPLE_SIZE; i++) {
    int res = pulseIn(sensor.pin, HIGH);
    while (res == 0 || res > 1000) {
      res = pulseIn(sensor.pin, HIGH);
    }
    samples[i] = res;
  }

  float res = filter_samples(samples);

  if (DEBUG) {
    Serial.print(sensor.name + " raw ranging:");
    for (int elem : samples) {
      Serial.print(String(elem) + ", ");
    }
    Serial.println("filtered: " + String(res));
  }


  if (res == 0 || isnan(res)) {
    //resample, could be infinite if sufficiently noisy
    delay(1000);
    return Claire::getRange(sensor);
  }
  
  if (conflicting_pump_old_duty != -1) {
    setPump(*conflicting_pump, conflicting_pump_old_duty);
    if (DEBUG && VERBOSE) {
        Serial.println("Started " + String(conflicting_pump->name) + " after ranging");
      }

  }
  return (int) res;

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


void Claire::loadEEPROMCalibration() {
  // eeprom test
  Serial.println("EEPROM: " + String(EEPROM.read(0)));
  EEPROM.write(0, 42);
  Serial.println("EEPROM: " + String(EEPROM.read(0)));
  EEPROM.write(0, 255);
}
