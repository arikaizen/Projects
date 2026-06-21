"""util_color_convert — Convert colors between hex, rgb, and hsl."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import re, math

def hex_to_rgb(h):
    h = h.lstrip("#")
    if len(h) == 3:
        h = "".join(c*2 for c in h)
    return tuple(int(h[i:i+2], 16) for i in (0, 2, 4))

def rgb_to_hex(r, g, b):
    return "#{:02X}{:02X}{:02X}".format(int(r), int(g), int(b))

def rgb_to_hsl(r, g, b):
    r, g, b = r/255, g/255, b/255
    cmax, cmin = max(r,g,b), min(r,g,b)
    delta = cmax - cmin
    l = (cmax + cmin) / 2
    s = 0 if delta == 0 else delta / (1 - abs(2*l - 1))
    if delta == 0:
        h = 0
    elif cmax == r:
        h = 60 * (((g - b) / delta) % 6)
    elif cmax == g:
        h = 60 * ((b - r) / delta + 2)
    else:
        h = 60 * ((r - g) / delta + 4)
    return round(h, 1), round(s * 100, 1), round(l * 100, 1)

def hsl_to_rgb(h, s, l):
    s, l = s/100, l/100
    c = (1 - abs(2*l - 1)) * s
    x = c * (1 - abs((h/60) % 2 - 1))
    m = l - c/2
    if h < 60:   r, g, b = c, x, 0
    elif h < 120: r, g, b = x, c, 0
    elif h < 180: r, g, b = 0, c, x
    elif h < 240: r, g, b = 0, x, c
    elif h < 300: r, g, b = x, 0, c
    else:         r, g, b = c, 0, x
    return round((r+m)*255), round((g+m)*255), round((b+m)*255)

def parse_color(color):
    color = color.strip()
    if color.startswith("#"):
        return "hex", hex_to_rgb(color)
    m = re.match(r"rgb\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\)", color, re.I)
    if m:
        return "rgb", (int(m.group(1)), int(m.group(2)), int(m.group(3)))
    m = re.match(r"hsl\(\s*([\d.]+)\s*,\s*([\d.]+)%\s*,\s*([\d.]+)%\s*\)", color, re.I)
    if m:
        return "hsl", (float(m.group(1)), float(m.group(2)), float(m.group(3)))
    raise ValueError(f"Cannot parse color: {color}")

def handler(args, keys):
    color = args["color"]
    to_format = args["to_format"].lower()
    fmt, vals = parse_color(color)

    # Convert to RGB first
    if fmt == "hex":
        rgb = vals
    elif fmt == "rgb":
        rgb = vals
    else:  # hsl
        rgb = hsl_to_rgb(*vals)

    if to_format == "rgb":
        return {"input": color, "result": f"rgb({rgb[0]},{rgb[1]},{rgb[2]})", "format": "rgb"}
    elif to_format == "hex":
        return {"input": color, "result": rgb_to_hex(*rgb), "format": "hex"}
    elif to_format == "hsl":
        h, s, l = rgb_to_hsl(*rgb)
        return {"input": color, "result": f"hsl({h},{s}%,{l}%)", "format": "hsl"}
    else:
        raise ValueError(f"Unknown target format: {to_format}")

run(handler)
