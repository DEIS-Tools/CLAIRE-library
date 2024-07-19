# LLM generated regex for stringified python dict
import re

# Compact regex pattern to match Python dictionaries
dict_pattern = re.compile(
    r'\{\s*"([a-zA-Z_][a-zA-Z0-9_]*)"\s*:\s*(-?\d+(\.\d+)?|"\w*")\s*(,\s*"([a-zA-Z_][a-zA-Z0-9_]*)"\s*:\s*(-?\d+(\.\d+)?|"\w*"))*\s*\}')
test_string = '{"Tube0_water_mm": -1.00, "Tube1_water_mm": -1.00, "Tube0_inflow_duty": 0,"Tube0_outflow_duty": 0,"Tube1_inflow_duty": 0,"Tube1_outflow_duty": 0,"Stream_inflow_duty": 0,"Stream_outflow_duty": 0}'


def is_dict(s: str) -> bool:
    """
    >>> is_dict(test_string)
    True

    >>> is_dict('{"a": 1, "b": 2}')
    True

    >>> is_dict('{no: way}')
    False
    """
    return bool(dict_pattern.match(s))


def parse_str_dict(s: str) -> dict:
    """
    >>> parse_str_dict(test_string)
    {'Tube0_water_mm': -1.0, 'Tube1_water_mm': -1.0, 'Tube0_inflow_duty': 0, 'Tube0_outflow_duty': 0, 'Tube1_inflow_duty': 0, 'Tube1_outflow_duty': 0, 'Stream_inflow_duty': 0, 'Stream_outflow_duty': 0}

    >>> parse_str_dict('{"a": 1, "b": 2}')
    {'a': 1, 'b': 2}

    >>> parse_str_dict('{no: way}')
    Traceback (most recent call last):
        ...
    ValueError: Not a valid dict string: "{no: way}"

    >>> parse_str_dict('')
    Traceback (most recent call last):
        ...
    ValueError: Not a valid dict string: ""
    """
    if is_dict(s):
        return eval(s)
    else:
        raise ValueError(f'Not a valid dict string: "{s}"')
