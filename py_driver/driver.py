import sys
import threading
from time import sleep, time
import serial
import utils

IMMEDIATE_OUTPUT = True
TAG = "DRIVER:"
CLAIRE_VERSION = "v0.1.11"
TUBE_MAX_LEVEL = 900
DEBUG = True


class ColorPrinting(object):
    """
    ANSI escape coding for printing colors.

    Usage: start string with the desired format escape code, end string with ENDC end coding.
    """
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
        self.stopped = False
        self.read_buffer = []
        self.last_printed_buf_line = -1
        self.read_thread = threading.Thread(target=self._read_lines)
        self.read_thread.daemon = True
        self.read_thread.start()
        print(f'{TAG} Device connected to {port}, waiting for initialization...')
        sleep(3)
        self.check_version()

    def _read_lines(self):
        """Read lines from the serial port and add to the buffer in a thread to not block the main thread."""
        while True:
            buf = self.ser.readlines()
            if buf:
                self.heartbeat = time()
                new_lines = [line.decode('utf-8').rstrip() for line in buf]
                self.read_buffer.extend(new_lines)
                if IMMEDIATE_OUTPUT:
                    self.print_new_lines_buf()
                # Check whether the new lines contain the finished signal.
                for line in new_lines:
                    if line == "Finished":
                        self.busy = False

            # Stop reading lines.
            if self.stopped:
                break

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

    def check_version(self):
        """Check the version of the software on the Arduino"""
        # Version number is on the first line, as that reads "Initialising CLAIRE water management <version>"
        line = self.read_buffer[0]
        words = line.split(' ')

        # Sanity checking
        assert words[0] == "Initialising"
        assert len(words) == 5

        # Check version
        assert words[-1] == CLAIRE_VERSION, f"The CLAIRE software on the Arduino is version {words[-1]}, while this " \
                                            f"Python script is constructed for version {CLAIRE_VERSION}."

    def get_state(self):
        """Get the last state of the device."""
        self.write('1;')

        # wait for the state to be received
        while True:
            sleep(3)  # Getting filtered state takes some time, each sensor reading takes 50-100 ms
            state = self.get_last_raw_state()
            if state:
                # Convert distance to water level
                state["Tube0_water_mm"] = round(self.convert_distance_to_level(state["Tube0_water_mm"]), 1)
                state["Tube1_water_mm"] = round(self.convert_distance_to_level(state["Tube1_water_mm"]), 1)
                return state

    def get_last_raw_state(self):
        """Get the last raw state of the device without polling"""
        # take buf backwards and try to coerce every line into dict
        for line in reversed(self.buf_lines()):
            try:
                state = utils.parse_str_dict(line)
                return state
            except ValueError:
                pass

    @staticmethod
    def print_state(state):
        """Print state of the system"""
        print(f'{TAG} Got state: {state}')

    def write(self, data):
        """Write data to the serial port."""
        if DEBUG:
            print(f'{TAG} Writing command: {data}')
        self.ser.write(data.encode('utf-8'))

    def close(self):
        self.stopped = True
        self.read_thread.join()  # Wait until read thread has been stopped.
        self.ser.close()

    def set_water_level(self, tube, level):
        """Set the water level in the selected tube to the provided height"""
        assert tube == 1 or tube == 2
        assert 0 <= level <= TUBE_MAX_LEVEL
        self.write(f"5 {tube} {self.convert_level_to_distance(level)};")
        self.busy = True

    def set_inflow(self, tube, rate):
        """Set the inflow in the tube to the provided rate"""
        assert tube == 1 or tube == 2
        assert 0 <= rate <= 100
        pump = (tube - 1) * 2 + 1
        self.write(f"4 {pump} {rate};")

    def set_outflow(self, tube, rate):
        """Set the outflow in the tube to the provided rate"""
        assert tube == 1 or tube == 2
        assert 0 <= rate <= 100
        pump = tube * 2
        self.write(f"4 {pump} {rate};")

    @staticmethod
    def convert_distance_to_level(distance):
        """Convert sensor distance to water level"""
        return TUBE_MAX_LEVEL - distance

    @staticmethod
    def convert_level_to_distance(level):
        """Convert water level to sensor distance"""
        return TUBE_MAX_LEVEL - level

    def wait_until_free(self):
        while True:
            if not self.busy:
                return
            sleep(1)
