#include "Claire.h"

Claire claire = Claire();

// setup default verbosity levels
bool VERBOSE = true;
bool DEBUG = true;

// use reference implementation pumps and sensors
using namespace default_pump_defs;
using namespace default_sensor_defs;

// Attach CmdMessenger object to the default Serial port, setting cmd sep. to whitespace
#include <CmdMessenger.h>
CmdMessenger cmdMessenger = CmdMessenger(Serial, ' ');

// This is the list of recognized commands.
// In order to receive, attach a callback function to these events
enum {
  kCommandList,  // Command to request list of available commands
  kStatus,       // Command to request status of system
  kStop,         // Command to stop all outputs
  kSetPump,      // Command to set pump to a duty cycle
  kSetLevel,     // Command to set water level in a given tube
  kPrime,        // Command to prime the pumps by cycling off, 100% n times
  kReset,        // Command to reset the system by emptying reservoirs
  kEmpty,        // Command to empty the demonstrator into bucket for tear-down
  kTest,         // Command to test output on all 
  kVerbosity     // Command to change verbosity of CLAIRE
};

// Callbacks define on which received commands we take action
void attachCommandCallbacks() {
  cmdMessenger.attach(OnUnknownCommand);
  cmdMessenger.attach(kCommandList, OnCommandList);
  cmdMessenger.attach(kStatus, OnStatus);
  cmdMessenger.attach(kStop, OnStop);
  cmdMessenger.attach(kSetPump, OnSetPump);
  cmdMessenger.attach(kSetLevel, OnSetLevel);
  cmdMessenger.attach(kPrime, OnPrime);
  cmdMessenger.attach(kReset, OnReset);
  cmdMessenger.attach(kEmpty, OnEmpty);
  cmdMessenger.attach(kTest, OnTest);
  cmdMessenger.attach(kVerbosity, OnVerbosity);
}

// Called when a received command has no attached function
void OnUnknownCommand() {
  Serial.println("This command is unknown!");
  ShowCommands();
}

void OnCommandList() {
  ShowCommands();
}

void OnStatus() {
  // Construct dict from demonstrator state and report at end.
  // Error reporting is handled by called functions, and sentinel (-1) is used for error state.
  String fmt = "{";

  for (int i = 0; i < claire.sensorCount; i++) {
    fmt += "\"" + String(claire.sensors[i].name) + "\": " + claire.getRange(claire.sensors[i]) + ", ";
  }

  for (int i = 0; i < claire.pumpCount; i++) {
    fmt += '"' + String(claire.pumps[i]->name) + String("_duty\": ") + String(claire.pumps[i]->duty);
    if (i == claire.pumpCount - 1) {
      fmt += '}';
    } else {
      fmt += ',';
    }
  }
  Serial.println(fmt);
}

void OnStop() {
  claire.eStop();
  Serial.println("Emergency stop applied! Expect state of demonstrator to be undefined.");
}

void OnSetPump() {
  // read args and sanitise
  int tube = cmdMessenger.readInt16Arg();
  int duty = cmdMessenger.readInt16Arg();

  if (0 > duty || duty > 100) {
    Serial.println("Duty (" + String(duty) + ") is out of bounds [0..100], ignoring command");
    return;
  }

  switch (tube) {
    case 1:
      claire.setPump(TUBE0_IN, duty);
      break;
    case 2:
      claire.setPump(TUBE0_OUT, duty);
      break;
    case 3:
      claire.setPump(TUBE1_IN, duty);
      break;
    case 4:
      claire.setPump(TUBE1_OUT, duty);
      break;
    case 5:
      claire.setPump(STREAM_IN, duty);
      break;
    case 6:
      claire.setPump(STREAM_OUT, duty);
      break;
    default:
      Serial.println("Unknown pump referenced (" + String(tube) + "), ignoring command");
      return;
  }
}

// sets water level in a tube to arg
void OnSetLevel() {
  // validate arg to be within 0..700 mm bound
  int tube = cmdMessenger.readInt16Arg();
  int level = cmdMessenger.readInt16Arg();

  if (0 > level || level > 700) {
    Serial.println("Level (" + String(level) + ") is out of bounds [0..700], ignoring command");
    return;
  }

  switch (tube) {
    case 1:
      claire.setLevel(TUBE0_IN, TUBE0_OUT, level);
      break;
    case 2:
      claire.setLevel(TUBE1_IN, TUBE1_OUT, level);
      break;
    default:
      Serial.println("Unknown tube '" + String(tube) + "' ignoring command");
      break;
  }
}

void OnPrime() {
  Serial.println("PRIMING: Make sure the minimum level of water is filled into the reservoir");

  bool pump = true;
  int cycles[] = { 500, 200,
                   500, 200,
                   500, 200,
                   2000, 1000,
                   2000, 500,
                   1000, 10 };

  for (auto ms : cycles) {
    if (pump) {
      Serial.print("prime: " + String(ms) + " ms, ");
      claire.setPump(TUBE0_IN, 100);
      claire.setPump(TUBE1_IN, 100);
      delay(ms);
    } else {
      Serial.print("delay: " + String(ms) + " ms, ");
      claire.setPump(TUBE0_IN, 0);
      claire.setPump(TUBE1_IN, 0);
      delay(ms);
    }
    pump = !pump;

    // reset system
    OnReset();
  }
}

// empty all containers into res.
void OnReset() {
}



void OnEmpty() {
  // first reset, then pump out of designated tube
  OnReset();
}

void OnTest() {
  claire.testOutput();
}

void OnVerbosity() {
  VERBOSE = cmdMessenger.readBoolArg();
  DEBUG = cmdMessenger.readBoolArg();
  claire.DEBUG = DEBUG;
  claire.VERBOSE = VERBOSE;
  Serial.println("Verbose: " + String(VERBOSE) + " debug: " + String(DEBUG));
}

// Show available commands
void ShowCommands() {
  Serial.println("Usage: cmd [args] ;");
  Serial.println(" 0;                  - This command list");
  Serial.println(" 1;                  - Status of system in k:v");
  Serial.println(" 2;                  - Emergency stop all actuators");
  Serial.println(" 3 <pump> <flow>;    - Set pump flow. 0 = off, 1..100 = proportional flow-rate");
  Serial.println("   <pump> = {1: TUBE0_IN, 2: TUBE0_OUT, 3: TUBE1_IN, 4: TUBE1_OUT, 5: STREAM_OUT}");
  Serial.println(" 4 <tube> <level>;   - Set tube level in millimeters.");
  Serial.println("   <tube> = {1: TUBE0, 2: TUBE1}");
  Serial.println(" 5;                  - Primes the pumps on a newly filled system");
  Serial.println(" 6;                  - Reset system: Empty all reservoirs, then turn all pumps off");
  Serial.println(" 7;                  - Tear-down: Empty the system and water into separate bucket");
  Serial.println(" 8;                  - Sweep PWM on outputs. Will require reset to quit");
  Serial.println(" 9 <verbose> <debug>;- Set verbosity and debug level (0=off, 1=on)");
}

// Setup function
void setup() {
  // Listen on serial connection for messages from the PC
  Serial.begin(115200);

  // Adds newline to every command
  cmdMessenger.printLfCr();

  // Attach my application's user-defined callback methods
  attachCommandCallbacks();

  // Init CLAIRE
  claire.VERBOSE = VERBOSE;
  claire.DEBUG = DEBUG;
  claire.begin();

  // Show command list
  ShowCommands();
}

// Loop function
void loop() {
  // Process incoming serial data, and perform callbacks
  cmdMessenger.feedinSerialData();
}
