# CLAIRE water management demonstrator
This repo holds the source code for the Arduino Library implemented for use by students and researchers to apply controllers to a model of two rain-water basins (illustrated by the tubes). 
The environment fills the basins, and the goal is to maximise the level in the tubes to increase sedimentation before discharging the water for later water-works, while minimising the risk of an overflow.
The controller knows the forecast, the level of the basin(s), and can choose to discharge some amount.

To model this, each tube has an inflow- and outflow-pump along with an ultrasonic sensor to measure with mm-precision the height of the water column. This is exposed with an interface to the user for control.

## Usage

### Installation
Install the library from the Arduino Library Manager by searching for 'CLAIRE'.

If using `Console.ino`, the following dependencies installed from the Arduino Library Manager are required:
- [CmdMessenger](https://github.com/thijse/Arduino-CmdMessenger/), v4.0.0 | `arduino-cli lib install "CmdMessenger"@4.0.0`
- [AceSorting](https://github.com/bxparks/AceSorting), v1.0.0 | `arduino-cli lib install "AceSorting"@1.0.0`

### Console-firmware
Flash `Console.ino` onto an Arduino Mega using the Arduino IDE. Use a serial device tool such as `tio` or `screen` to connect to the Arduino with serial over USB. On Linux, given the virtual terminal path `/dev/ttyUSB0` and a baud rate of `115200`, do: `tio -b 115200 /dev/ttyUSB0`.

Once connected (on v0.1.9), you are presented with the following usage-message:
```
$ tio -b 115200 /dev/ttyUSB0
[11:24:19.157] tio v3.3
[11:24:19.157] Press ctrl-t q to quit
[11:24:19.215] Connected to /dev/ttyUSB0
Initialising CLAIRE water management v0.1.9
Usage: cmd [args] ;
 0;                  - This command list
 1;                  - Status of system in k:v
 2;                  - Quick status (1 sample) of system in k:v
 3;                  - Emergency stop all actuators
 4 <pump> <flow>;    - Set pump flow. 0 = off, 1..100 = proportional flow-rate
   <pump> = {1: TUBE1_IN, 2: TUBE1_OUT, 3: TUBE2_IN, 4: TUBE2_OUT, 5: STREAM_OUT}
 5 <tube> <level>;   - Set tube level in millimeters.
   <tube> = {1: TUBE1, 2: TUBE2}
 6;                  - Primes the pumps on a newly filled system
 7;                  - Reset system: Empty all reservoirs, then turn all pumps off
 8;                  - Tear-down: Empty the system and water into separate bucket
 9;                  - Sweep PWM on outputs. Will require reset to quit
10 <verbose> <debug>;- Set verbosity and debug level (0=off, 1=on)
```

Actuate on the demonstrator by passing commands, e.g. to run inflow of tube 0 at 40% duty, pass: `4 1 40;`

Get the state of the system by passing: `1;` or `2;`. You get returned an associative array with the state of the system, example below. Failure to acquire a clean signal on the height of the water column in a tube is represented by the sentiel `-1.00`.
```
{"Tube0_water_mm": 572.23, "Tube1_water_mm": -1.00, "Tube0_inflow_duty": 40,"Tube0_outflow_duty": 0,"Tube1_inflow_duty": 100,"Tube1_outflow_duty": 20,"Stream_outflow_duty": 0}
```

Set the desired level of water in a tube by using command `5`. This will fill the tube using the ranging as feedback, until it reaches the desired level in millimeters.

### System dynamics
Equilibrium between inflow and outflow at max tube level is obtained at system state:
```py
{"Tube0_water_mm": 296.60, "Tube1_water_mm": 295.50, "Tube0_inflow_duty": 100,"Tube0_outflow_duty": 30,"Tube1_inflow_duty": 100,"Tube1_outflow_duty": 30,"Stream_inflow_duty": 0,"Stream_outflow_duty": 0}
```

### Known issues
The ultrasonic sensors are MatBotix MB7360 HRXL-MaxSonar-WR, which are tuned for picking up small targets close to the sensor.
The sensitivity is too high for the system and not tunable. The amount of protrusion of fittings in the upper part of the tubes have a high effect on false readings. Changing the model to the MB7368, which is tuned for _largest target_, could be a more reliable choice. 

### Python driver
A Python driver is available in the `py_driver/` directory. It uses the `pyserial` library to communicate with the Arduino over serial. The driver is a simple wrapper around the serial communication, and can be used to send commands to the Arduino and receive responses.
The API does not cover all commands on the CLAIRE-firmware, but can be easily extended. Example usage is shown below:
```python
if __name__ == '__main__':
    claire = ClaireDevice(PORT)
    state = claire.get_state()  # get current state of device
    print(f'{TAG} Current height of TUBE0: {state["Tube0_water_mm"]}')

    claire.write('7 1 500;')  # set level to 500mm in first tube
```

### Custom firmware

See `Basic.ino` example bundled with the library for getting started writing your own firmware.

```c
#include "Claire.h"

// use default definitions of pumps and sensors of reference physical implementation
using namespace default_pump_defs;
using namespace default_sensor_defs;

Claire claire = Claire();

void setup() {
  // set debug to be verbose in output
  claire.DEBUG = true;

  // setup 9600 baud for serial connection
  Serial.begin(9600);

  claire.begin();
}

void loop() {
  bool ok;
  
  // get height of water column in tube0
  int tube0_height = claire.getRange(TUBE0_HEIGHT);

  // pump with 100% flow into tube0 for five seconds
  ok &= claire.setPump(TUBE0_IN, 100);
  delay(5000);
  ok &= claire.setPump(TUBE0_IN, 0);

  Serial.println("Tube0 gained " + String(tube0_height - claire.getRange(TUBE0_HEIGHT)) + " mm of water");

  digitalWrite(LED_BUILTIN, !ok);
  delay(5000);
}
```

## CAD model
![CAD model of CLAIRE demonstrator v1](figures/cad-v1.png)
