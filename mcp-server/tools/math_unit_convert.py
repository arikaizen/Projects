"""math_unit_convert — Convert between common units."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run

# All values converted to SI base, then to target
LENGTH_TO_M = {"m": 1, "km": 1000, "cm": 0.01, "mm": 0.001,
               "ft": 0.3048, "in": 0.0254, "mi": 1609.344, "yd": 0.9144}
WEIGHT_TO_KG = {"kg": 1, "g": 0.001, "lb": 0.453592, "oz": 0.0283495}
VOLUME_TO_L = {"L": 1, "mL": 0.001, "gal": 3.78541, "fl_oz": 0.0295735}
SPEED_TO_MS = {"ms": 1, "kph": 1/3.6, "mph": 0.44704}

CATEGORY_MAPS = {
    "length": LENGTH_TO_M,
    "weight": WEIGHT_TO_KG,
    "volume": VOLUME_TO_L,
    "speed": SPEED_TO_MS,
}

def find_category(unit):
    for cat, mapping in CATEGORY_MAPS.items():
        if unit in mapping:
            return cat, mapping
    return None, None

def convert_temp(value, from_unit, to_unit):
    if from_unit == to_unit:
        return value
    # to Celsius first
    if from_unit == "F":
        c = (value - 32) * 5 / 9
    elif from_unit == "K":
        c = value - 273.15
    else:
        c = value
    if to_unit == "F":
        return c * 9 / 5 + 32
    if to_unit == "K":
        return c + 273.15
    return c

def handler(args, keys):
    value = float(args["value"])
    from_unit = args["from_unit"]
    to_unit = args["to_unit"]

    temp_units = {"C", "F", "K"}
    if from_unit in temp_units or to_unit in temp_units:
        if from_unit not in temp_units or to_unit not in temp_units:
            raise ValueError("Cannot convert between temperature and non-temperature units")
        result = convert_temp(value, from_unit, to_unit)
        return {"value": value, "from_unit": from_unit, "result": result, "to_unit": to_unit,
                "label": f"{value} {from_unit} = {result} {to_unit}"}

    from_cat, from_map = find_category(from_unit)
    to_cat, to_map = find_category(to_unit)
    if from_cat is None:
        raise ValueError(f"Unknown unit: {from_unit}")
    if to_cat is None:
        raise ValueError(f"Unknown unit: {to_unit}")
    if from_cat != to_cat:
        raise ValueError(f"Cannot convert {from_cat} to {to_cat}")

    base = value * from_map[from_unit]
    result = base / to_map[to_unit]
    return {"value": value, "from_unit": from_unit, "result": result, "to_unit": to_unit,
            "label": f"{value} {from_unit} = {result} {to_unit}"}

run(handler)
