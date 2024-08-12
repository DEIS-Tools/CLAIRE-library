#include "Claire.h"

Claire claire = Claire();

// setup default verbosity levels
bool VERBOSE = true;
bool DEBUG = false;

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
  kQuickStatus,  // Command to request a quick status (one sample) of system
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
  cmdMessenger.attach(kQuickStatus, OnQuickStatus);
  cmdMessenger.attach(kStop, OnStop);
  cmdMessenger.attach(kSetPump, OnSetPump);
  cmdMessenger.attach(kSetLevel, OnSetLevel);
  cmdMessenger.attach(kPrime, OnPrime);
  cmdMessenger.attach(kReset, OnReset);
  cmdMessenger.attach(kEmpty, OnEmpty);
  cmdMessenger.attach(kTest, OnTest);
  cmdMessenger.attach(kVerbosity, OnVerbosity);
}

void OnReady() {
  Serial.println("CLAIRE-READY");
}

// Called when a received command has no attached function
void OnUnknownCommand() {
  Serial.println("This command is unknown!");
  ShowCommands();
  OnReady();
}

void OnCommandList() {
  ShowCommands();
  OnReady();
}

String getStatus(bool filtered, int tube = 0) {
  // Construct dict from demonstrator state and report at end.
  // Error reporting is handled by called functions, and sentinel (-1) is used for error state.

  if (0 > tube || tube > 2) {
    return "Error: Tube index is out of bounds [1..2], for both use value of 0. Received: " + String(tube);
  }

  String fmt = "{";

  if (tube == 0) {
    for (int i = 0; i < claire.sensorCount; i++) {
      fmt += "\"" + String(claire.sensors[i].name) + "\": " + claire.getRange(claire.sensors[i], filtered) + ", ";
    }
  }

  if (1 <= tube && tube <= 2) {
    fmt += "\"" + String(claire.sensors[tube - 1].name) + "\": " + claire.getRange(claire.sensors[tube - 1], filtered) + ", ";
  }

  for (int i = 0; i < claire.pumpCount; i++) {
    fmt += '"' + String(claire.pumps[i]->name) + String("_duty\": ") + String(claire.pumps[i]->duty);
    if (i == claire.pumpCount - 1) {
      fmt += '}';
    } else {
      fmt += ',';
    }
  }
  return fmt;
}

void OnStatus() {
  // do default samples
  int tube = 0;
  if (cmdMessenger.isArgOk()) {
    tube = cmdMessenger.readInt16Arg();
    Serial.println("Got tube: " + String(tube));
  }
  Serial.println(getStatus(true, tube));
  OnReady();
}

void OnQuickStatus() {
  // only do one sample
  int tube = 0;
  if (cmdMessenger.isArgOk()) {
    tube = cmdMessenger.readInt16Arg();
  }
  Serial.println(getStatus(false, tube));
  OnReady();
}

void OnStop() {
  claire.eStop();
  Serial.println("Emergency stop applied! Expect state of demonstrator to be undefined.");
  OnReady();
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
      claire.setPump(TUBE1_IN, duty);
      break;
    case 2:
      claire.setPump(TUBE1_OUT, duty);
      break;
    case 3:
      claire.setPump(TUBE2_IN, duty);
      break;
    case 4:
      claire.setPump(TUBE2_OUT, duty);
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
  OnReady();
}

// sets water level in a tube to arg
void OnSetLevel() {
  int tube = cmdMessenger.readInt16Arg();
  int level = cmdMessenger.readInt16Arg();

  // fixme: add guard for other end of range when auto-calibration is implemented
  // validate arg to be within 0..MAX mm bound
  if (0 > level || level > TUBE_MAX_LEVEL) {
    Serial.println("Level (" + String(level) + ") is out of bounds [0.." + String(TUBE_MAX_LEVEL) + "], ignoring command");
    // Notify that command is finished.
    Serial.println("Finished");
    return;
  }

  switch (tube) {
    case 1:
      claire.setLevel(TUBE1_IN, TUBE1_OUT, level);
      Serial.println("TUBE1 range: " + String(claire.getRange(claire.sensors[0])));
      break;
    case 2:
      claire.setLevel(TUBE2_IN, TUBE2_OUT, level);
      Serial.println("TUBE2 range: " + String(claire.getRange(claire.sensors[1])));
      break;
    default:
      Serial.println("Unknown tube '" + String(tube) + "' ignoring command");
      break;
  }

  // Notify that command is finished.
  OnReady();
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
      claire.setPump(TUBE1_IN, 100);
      claire.setPump(TUBE2_IN, 100);
      delay(ms);
    } else {
      Serial.print("delay: " + String(ms) + " ms, ");
      claire.setPump(TUBE1_IN, 0);
      claire.setPump(TUBE2_IN, 0);
      delay(ms);
    }
    pump = !pump;

    // reset system
    OnReset();
  }

  // Notify that command is finished.
  OnReady();
}

// empty all containers into res.
void OnReset() {
  Serial.println("Emptying all tubes into reservoir");
  if (VERBOSE) Serial.println("Emptying TUBE0");
  bool ok_tube1 = claire.setLevel(TUBE1_IN, TUBE1_OUT, TUBE_MAX_LEVEL);
  if (VERBOSE) Serial.println("Emptying TUBE1");
  bool ok_tube2 = claire.setLevel(TUBE2_IN, TUBE2_OUT, TUBE_MAX_LEVEL);
  if (ok_tube1 && ok_tube2) {
    Serial.println("All tubes emptied successfully");
  }
  OnReady();
}


void OnEmpty() {
  // first reset, then pump out of designated tube
  OnReset();
  Serial.println("WARN: Running pump: " + String(TUBE1_IN.name) + " at 100% until user resets!");
  claire.setPump(TUBE1_IN, 100);
  OnReady();
}

void OnTest() {
  claire.testOutput();
  OnReady(); // not reachable unless err
}

void OnVerbosity() {
  VERBOSE = cmdMessenger.readBoolArg();
  DEBUG = cmdMessenger.readBoolArg();
  claire.DEBUG = DEBUG;
  claire.VERBOSE = VERBOSE;
  Serial.println("Verbose: " + String(VERBOSE) + " debug: " + String(DEBUG));
  OnReady();
}

// Show available commands
void ShowCommands() {
  Serial.println("Usage: cmd [args] ;");
  Serial.println(" 0;                  - This command list");
  Serial.println(" 1;                  - Status of system in k:v");
  Serial.println(" 2;                  - Quick status (1 sample) of system in k:v");
  Serial.println(" 3;                  - Emergency stop all actuators");
  Serial.println(" 4 <pump> <flow>;    - Set pump flow. 0 = off, 1..100 = proportional flow-rate");
  Serial.println("   <pump> = {1: TUBE1_IN, 2: TUBE1_OUT, 3: TUBE2_IN, 4: TUBE2_OUT, 5: STREAM_OUT}");
  Serial.println(" 5 <tube> <level>;   - Set tube level in millimeters.");
  Serial.println("   <tube> = {1: TUBE1, 2: TUBE2}");
  Serial.println(" 6;                  - Primes the pumps on a newly filled system");
  Serial.println(" 7;                  - Reset system: Empty all reservoirs, then turn all pumps off");
  Serial.println(" 8;                  - Tear-down: Empty the system and water into separate bucket");
  Serial.println(" 9;                  - Sweep PWM on outputs. Will require reset to quit");
  Serial.println("10 <verbose> <debug>;- Set verbosity and debug level (0=off, 1=on)");
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

  // Give ready signal when init is finished
  OnReady();
}

// Loop function
void loop() {
  // Process incoming serial data, and perform callbacks
  cmdMessenger.feedinSerialData();
}
