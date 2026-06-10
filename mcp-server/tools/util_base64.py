"""util_base64 — Base64 encode or decode."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import base64

def handler(args, keys):
    mode = args.get("mode", "encode")
    text = args["text"]
    if mode == "encode":
        variant = args.get("variant", "standard")
        if variant == "urlsafe":
            return {"result": base64.urlsafe_b64encode(text.encode()).decode()}
        return {"result": base64.b64encode(text.encode()).decode()}
    else:
        try:
            return {"result": base64.b64decode(text + "==").decode("utf-8")}
        except Exception:
            return {"result": base64.urlsafe_b64decode(text + "==").decode("utf-8")}

run(handler)
