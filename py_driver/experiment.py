import driver
from time import sleep

# Insert the name of the usb port, which might be different for different devices.
# An easy way to get the port name is to use the Arduino IDE.
# PORT = '/dev/ttyUSB0'
PORT = '/dev/cu.usbserial-1420'


def example_experiment():
    claire = driver.ClaireDevice(PORT)
    state = claire.update_state()  # get current state of device
    claire.print_state()
    print(f'Current height of TUBE1: {state["Tube1_water_mm"]}')

    claire.set_inflow(1, 100)
    sleep(3)
    claire.set_inflow(1, 0)
    sleep(3)
    claire.set_outflow(1, 100)
    sleep(3)
    claire.set_outflow(1, 0)
    claire.set_water_level(1, 500)  # set water level to 500mm in first tube

    # wait forever or until KeyboardInterrupt
    try:
        print(f'Only monitoring. Press Ctrl+C to exit')
        while True:
            sleep(1)
    except KeyboardInterrupt:
        claire.close()
        print(f'Got interrupt. Exiting...')


if __name__ == '__main__':
    example_experiment()