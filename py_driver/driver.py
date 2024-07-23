import sys
import threading
from time import sleep, time

import serial
import utils

# PORT = '/dev/ttyUSB0'
PORT = '/dev/cu.usbserial-1420'
IMMEDIATE_OUTPUT = True
TAG = "DRIVER:"


class ColorPrinting(object):
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

    @staticmethod
    def print_blue(text):
        print(f"{ColorPrinting.OKBLUE}{text}{ColorPrinting.ENDC}")


class ClaireDevice:
    def __init__(self, port):
        self.device = port
        self.heartbeat = time()
        self.busy = False
        # read timeout in secs, 1 should be sufficient

        # exclusive only available on posix-like systems, assumes mac-env is posix-like
        if ["linux", "darwin"].__contains__(sys.platform):
            exclusive = True
        else:
            exclusive = False

        self.ser = serial.Serial(port, 115200, timeout=1, exclusive=exclusive)

        # buffer of entire run
        self.read_buffer = []
        self.last_printed_buf_line = -1
        self.read_thread = threading.Thread(target=self._read_lines)
        self.read_thread.daemon = True
        self.read_thread.start()
        print(f'{TAG} Device connected to {port}, waiting for initialization...')
        sleep(3)

    def _read_lines(self):
        """Read lines from the serial port and add to the buffer in a thread to not block the main thread."""
        while True:
            buf = self.ser.readlines()
            if buf:
                self.heartbeat = time()
                self.read_buffer.extend([line.decode('utf-8').rstrip() for line in buf])
                if IMMEDIATE_OUTPUT:
                    self.print_new_lines_buf()

    def buf_lines(self) -> list[str]:
        """Return the lines in the buffer"""
        return self.read_buffer[:]

    def buf_lines_from(self, start) -> list[str]:
        """Return the lines in the buffer from start"""
        return self.read_buffer[start:]

    def last_buf_lines(self) -> list[str]:
        """Return the lines in the buffer from last_printed_buf_line"""
        return self.buf_lines_from(self.last_printed_buf_line + 1)

    def print_buf(self):
        """Print the lines in the buffer in blue to differentiate from experiment output."""
        # print to stderr to get colour
        for line in self.buf_lines():
            ColorPrinting.print_blue(line)

    def print_last_line_buf(self):
        """Prints the last line in the buffer in blue to differentiate from experiment output."""
        # print to stderr to get colour
        ColorPrinting.print_blue(self.read_buffer[-1])

    def print_new_lines_buf(self):
        """
        Prints the new (not yet printed) lines in the buffer in blue to differentiate from experiment output.
        """
        # print to stderr to get colour
        if len(self.read_buffer) > self.last_printed_buf_line + 1:
            for line in self.last_buf_lines():
                ColorPrinting.print_blue(line)
            self.last_printed_buf_line = len(self.read_buffer) - 1

    def get_state(self):
        """Get the last state of the device."""
        self.write('1;')

        # wait for the state to be received
        while True:
            sleep(1)
            state = self.get_last_state()
            if state:
                return state

    def get_last_state(self):
        """Get the last state of the device without polling"""
        # take buf backwards and try to coerce every line into dict
        for line in reversed(self.buf_lines()):
            try:
                state = utils.parse_str_dict(line)
                return state
            except ValueError:
                pass

    def print_state(self, state):
        """Print state of the system"""
        print(f'{TAG} Got state: {state}')

    def write(self, data):
        """Write data to the serial port."""
        print(f'{TAG} Writing command: {data}')
        self.ser.write(data.encode('utf-8'))

    def close(self):
        self.ser.close()


if __name__ == '__main__':
    claire = ClaireDevice(PORT)
    state = claire.get_state()  # get current state of device
    claire.print_state(state)
    print(f'{TAG} Current height of TUBE0: {state["Tube0_water_mm"]}')

    claire.write('5 1 500;')  # set level to 500mm in first tube
    claire.busy = True  # device becomes busy until level is set, unsetting upon validating return

    # wait forever or until KeyboardInterrupt
    try:
        print(f'{TAG} Only monitoring. Press Ctrl+C to exit')
        while True:
            sleep(1)
    except KeyboardInterrupt:
        claire.close()
        print(f'{TAG} Got interrupt. Exiting...')
