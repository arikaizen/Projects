"""util_json_validate — Parse, validate, and pretty-print JSON."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import json

def handler(args, keys):
    text   = args["text"]
    indent = int(args.get("indent", 2))
    try:
        parsed = json.loads(text)
        return {"valid": True, "formatted": json.dumps(parsed, indent=indent), "type": type(parsed).__name__}
    except json.JSONDecodeError as e:
        return {"valid": False, "error": str(e)}

run(handler)
