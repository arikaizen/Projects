"""util_time_convert — Convert timestamps between timezones and formats."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
from datetime import datetime, timezone
import time

def handler(args, keys):
    ts     = args.get("timestamp")  # ISO or unix epoch
    to_tz  = args.get("to_timezone", "UTC")  # simplified: only UTC + offset
    fmt    = args.get("output_format", "iso")

    if ts is None:
        dt = datetime.now(timezone.utc)
    elif isinstance(ts, (int, float)):
        dt = datetime.fromtimestamp(float(ts), tz=timezone.utc)
    else:
        try:
            dt = datetime.fromisoformat(str(ts).replace("Z","+00:00"))
        except Exception:
            dt = datetime.strptime(str(ts), "%Y-%m-%d %H:%M:%S").replace(tzinfo=timezone.utc)

    result = {"utc": dt.strftime("%Y-%m-%dT%H:%M:%SZ"),
              "unix_epoch": int(dt.timestamp()),
              "weekday": dt.strftime("%A"),
              "iso": dt.isoformat()}
    return result

run(handler)
