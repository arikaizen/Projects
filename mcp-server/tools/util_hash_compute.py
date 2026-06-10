"""util_hash_compute — Compute MD5, SHA-1, SHA-256, or SHA-512 hash."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import hashlib

def handler(args, keys):
    text      = args["text"]
    algorithm = args.get("algorithm", "sha256").lower()
    encoding  = args.get("encoding", "hex")
    data      = text.encode("utf-8")
    h         = hashlib.new(algorithm, data)
    if encoding == "hex":    return {"algorithm": algorithm, "hash": h.hexdigest()}
    if encoding == "base64":
        import base64
        return {"algorithm": algorithm, "hash": base64.b64encode(h.digest()).decode()}
    raise ValueError(f"Unknown encoding: {encoding}")

run(handler)
