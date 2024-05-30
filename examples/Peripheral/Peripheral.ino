#include "Claire.h"

Claire claire = Claire();
using namespace default_pump_defs;
using namespace default_sensor_defs;

// Below is an example of interacting with the sample:
//
//   Available commands
//   0;                  - This command list
//   1,<led state>;      - Set led. 0 = off, 1 = on
//   2,<led brightness>; - Set led brighness. 0 - 1000
//   3;                  - Show led state
//
// Command> 3;
//
//  Led status: on
//  Led brightness: 500
//
// Command> 2,1000;
//
//   Led status: on
//   Led brightness: 1000
//
// Command> 1,0;
//
//   Led status: off
//   Led brightness: 1000


#include <CmdMessenger.h>  // CmdMessenger

// PWM timing variables
unsigned long intervalOn = 0;
unsigned long prevBlinkTime = 0;
const unsigned long PWMinterval = 1000;

// Blinking led variables
bool ledState = 1;                      // On/Off state of Led
int ledBrightness = prevBlinkTime / 2;  // 50 % Brightness
const int kBlinkLed = 13;               // Pin of internal Led

// Attach a new CmdMessenger object to the default Serial port
CmdMessenger cmdMessenger = CmdMessenger(Serial);

// This is the list of recognized commands.
// In order to receive, attach a callback function to these events
enum {
  kCommandList,  // Command to request list of available commands
  kStatus,       // Command to request status of system
  kSetPump,      // Command to set pump to a duty cycle
  kPrime,        // Command to prime the pumps by cycling off, 100% n times
  kReset,        // Command to reset the system by emptying reservoirs
  kEmpty,        // Command to empty the demonstrator into bucket for tear-down
};

// Callbacks define on which received commands we take action
void attachCommandCallbacks() {
  // Attach callback methods
  cmdMessenger.attach(OnUnknownCommand);
  cmdMessenger.attach(kCommandList, OnCommandList);
  cmdMessenger.attach(kStatus, OnStatus);
  cmdMessenger.attach(kSetPump, OnSetPump);
  cmdMessenger.attach(kPrime, OnPrime);
  cmdMessenger.attach(kReset, OnReset);
  cmdMessenger.attach(kEmpty, OnEmpty);
  
}

// Called when a received command has no attached function
void OnUnknownCommand() {
  Serial.println("This command is unknown!");
  ShowCommands();
}

// Callback function that shows a list of commands
void OnCommandList() {
  ShowCommands();
}

// Callback function that shows led status
void OnStatus() {
  // Send back status that describes the led state
  //ShowLedState();

  String fmt = "{";
  

  for (int i = 0; i < claire.sensorCount; i++) {
    //fmt += '"' + String(claire.sensors[i]->name) + '": ' + claire.getRange(claire.sensors[i]) + ",";
  }



  for (int i = 0; i < claire.pumpCount; i++) {
    fmt += '"' + String(claire.pumps[i]->name) + '":' + claire.pumps[i]->duty + ",";
    if (i == claire.pumpCount - 1) {
        fmt += '}';
    } else {
      fmt += ',';
    }
  }

  Serial.println(fmt);
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
      claire.setPump(STREAM_OUT, duty);
      break;
    default:
      Serial.println("Unknown pump referenced (" + String(tube) + "), ignoring command");
      return;
  }
}

void OnPrime() {

}

void OnReset() {

}

void OnEmpty() {

}

// Callback function that sets led on or off
void OnSetLed() {
  // Read led state argument, expects 0 or 1 and interprets as false or true
  ledState = cmdMessenger.readBoolArg();
  ShowLedState();
}

// Callback function that sets led on or off
void OnSetLedBrightness() {
  // Read led brightness argument, expects value between 0 to 255
  ledBrightness = cmdMessenger.readInt16Arg();
  // Set led brightness
  SetBrightness();
  // Show Led state
  ShowLedState();
}


// Show available commands
void ShowCommands() {
  Serial.println("Usage: cmd [args] ;");
  Serial.println(" 0;                 - This command list");
  Serial.println(" 1;                 - Status of system in k:v");
  Serial.println(" 2, <pump>, <flow>; - Set pump flow. 0 = off, 1..100 = proportional flow-rate");
  Serial.println("    <pump> = {1: TUBE0_IN, 2: TUBE0_OUT, 3: TUBE1_IN, 4: TUBE1_OUT, 5: STREAM_OUT}");
  Serial.println(" 4;                 - Primes the pumps on a newly filled system");
  Serial.println(" 5;                 - Reset system: Empty all reservoirs, then turn all pumps off");
  Serial.println(" 6;                 - Tear-down: Empty the system and water into separate bucket");
}

// Show led state
void ShowLedState() {
  Serial.print("Led status: ");
  Serial.println(ledState ? "on" : "off");
  Serial.print("Led brightness: ");
  Serial.println(ledBrightness);
}

// Set led state
void SetLedState() {
  if (ledState) {
    // If led is turned on, go to correct brightness using analog write
    analogWrite(kBlinkLed, ledBrightness);
  } else {
    // If led is turned off, use digital write to disable PWM
    digitalWrite(kBlinkLed, LOW);
  }
}

// Set led brightness
void SetBrightness() {
  // clamp value intervalOn on 0 and PWMinterval
  intervalOn = max(min(ledBrightness, PWMinterval), 0);
}

// Pulse Width Modulation to vary Led intensity
// turn on until intervalOn, then turn off until PWMinterval
bool blinkLed() {
  if (micros() - prevBlinkTime > PWMinterval) {
    // Turn led on at end of interval (if led state is on)
    prevBlinkTime = micros();
    digitalWrite(kBlinkLed, ledState ? HIGH : LOW);
  } else if (micros() - prevBlinkTime > intervalOn) {
    // Turn led off at  halfway interval
    digitalWrite(kBlinkLed, LOW);
  }
}

// Setup function
void setup() {
  // Listen on serial connection for messages from the PC
  Serial.begin(115200);

  // Adds newline to every command
  cmdMessenger.printLfCr();

  // Attach my application's user-defined callback methods
  attachCommandCallbacks();

  // set pin for blink LED
  pinMode(kBlinkLed, OUTPUT);

  // Init CLAIRE
  claire.begin();

  // Show command list
  ShowCommands();
}

// Loop function
void loop() {
  // Process incoming serial data, and perform callbacks
  cmdMessenger.feedinSerialData();
  blinkLed();
}
