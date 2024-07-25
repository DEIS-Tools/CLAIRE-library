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
COMMUNICATION_TIMEOUT = 10


class SensorError(Exception):
    """
    Error when sensor reading fails (i.e., returns -1).
    """
    pass


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
        """
        Print the text in blue to standard output.

        :param text: The text to print.
        """
        print(f"{ColorPrinting.OKBLUE}{text}{ColorPrinting.ENDC}")


class ClaireState:
    """
    The state of the Claire demonstrator. Can be used to cache the state.
    """
    def __init__(self):
        self.state = None
        self.outdated = True

    def set_state(self, state):
        """
        Set the cached state to the provided state. Assumes that the provided state is actual.

        :param state: The new state to cache.
        """
        self.state = state
        self.outdated = state["Tube0_inflow_duty"] or state["Tube0_outflow_duty"] or state["Tube1_inflow_duty"] or state["Tube1_outflow_duty"]

    def make_outdated(self):
        """Label the cached state as outdated."""
        self.outdated = True


class ClaireDevice:
    """
    Class that represents the Claire demonstrator setup.
    """
    def __init__(self, port):
        self.device = port
        self.heartbeat = time()
        self.busy = False
        self.state = ClaireState()
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
        """
        Return the lines in the buffer.

        :return: The lines in the buffer."""
        return self.read_buffer[:]

    def buf_lines_from(self, start) -> list[str]:
        """
        Return the lines in the buffer from the provided start.

        :param: The start index to read the buffer.
        :return: The content of the buffer from start."""
        return self.read_buffer[start:]

    def last_buf_lines(self) -> list[str]:
        """
        Return the lines in the buffer from :attr:`self.last_printed_buf_line`.

        :return: The content of the buffer from :attr:`self.last_printed_buf_line`."""
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
        """Check the version of the software on the Arduino."""
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
        """Get the last state of the device. If cached state is outdated, a new sensor reading is requested."""
        # Return cached state if not outdated.
        if not self.state.outdated:
            return self.state.state

        # Ask for new state reading.
        size_buffer = self.last_printed_buf_line
        self.write('1;')

        # Wait for the state to be received.
        total_wait = 0
        while True:
            if self.last_printed_buf_line > size_buffer and self.read_buffer[-1][0] == '{':
                # If we received a line starting with {, we have received the new state.
                break

            sleep(0.1)
            total_wait += 0.1
            if total_wait > COMMUNICATION_TIMEOUT:
                raise RuntimeError("Waiting too long for state to be communicated.")

        # New state retrieved, parse it.
        state = self.get_last_raw_state()
        if state:
            # Convert distance to water level
            state["Tube0_water_mm"] = round(self.convert_distance_to_level(state["Tube0_water_mm"]), 1)
            state["Tube1_water_mm"] = round(self.convert_distance_to_level(state["Tube1_water_mm"]), 1)
            self.state = ClaireState()
            self.state.set_state(state)
            return state

    def get_last_raw_state(self):
        """Get the last raw state of the device without polling."""
        # take buf backwards and try to coerce every line into dict
        for line in reversed(self.buf_lines()):
            try:
                state = utils.parse_str_dict(line)
                return state
            except ValueError:
                pass

    def print_state(self):
        """Print state of the system."""
        print(f'{TAG} Got state: {self.state}')

    def write(self, data):
        """
        Write data to the serial port.

        :param data: The data to write to the serial port.
        """
        # Can only send new command if Arduino is not busy.
        assert not self.busy
        if DEBUG:
            print(f'{TAG} Writing command: {data}')
        self.ser.write(data.encode('utf-8'))
        sleep(0.1)  # Sleep slightly to take communication delays into account

    def close(self):
        """Close the serial connection."""
        self.stopped = True
        self.read_thread.join()  # Wait until read thread has been stopped.
        self.ser.close()

    def set_water_level(self, tube, level):
        """
        Set the water level in the selected tube to the provided height.

        :param tube: The tube to set the water level, should be value from [1,2].
        :param level: The desired water level, should be value from [0,TUBE_MAX_LEVEL].
        """
        assert tube == 1 or tube == 2
        assert 0 <= level <= TUBE_MAX_LEVEL
        self.write(f"5 {tube} {self.convert_level_to_distance(level)};")
        self.busy = True
        self.state.make_outdated()

    def set_inflow(self, tube, rate):
        """
        Set the inflow in the tube to the provided rate.

        :param tube: The tube to set the water level, should be value from [1,2].
        :param rate: The desired inflow rate, should be value from [0,100].
        """
        assert tube == 1 or tube == 2
        assert 0 <= rate <= 100
        pump = (tube - 1) * 2 + 1
        self.write(f"4 {pump} {rate};")
        self.state.make_outdated()

    def set_outflow(self, tube, rate):
        """
        Set the outflow in the tube to the provided rate.

        :param tube: The tube to set the water level, should be value from [1,2].
        :param rate: The desired outflow rate, should be value from [0,100].
        """
        assert tube == 1 or tube == 2
        assert 0 <= rate <= 100
        pump = tube * 2
        self.write(f"4 {pump} {rate};")
        self.state.make_outdated()

    @staticmethod
    def convert_distance_to_level(distance):
        """
        Convert sensor distance to water level.

        :param distance: The distance from the sensor to the measured water surface.
        """
        if distance < 0:
            raise SensorError()
        return TUBE_MAX_LEVEL - distance

    @staticmethod
    def convert_level_to_distance(level):
        """
        Convert water level to sensor distance.

        :param level: The water level to convert.
        """
        return TUBE_MAX_LEVEL - level

    def wait_until_free(self):
        """Wait until the device is free."""
        while True:
            if not self.busy:
                return
            sleep(1)
