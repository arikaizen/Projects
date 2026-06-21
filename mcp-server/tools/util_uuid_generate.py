"""util_uuid_generate — Generate UUIDs."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import uuid

def handler(args, keys):
    version = int(args.get("version", 4))
    count = min(int(args.get("count", 1)), 20)
    results = []
    for _ in range(count):
        if version == 1:
            results.append(str(uuid.uuid1()))
        elif version == 4:
            results.append(str(uuid.uuid4()))
        else:
            raise ValueError(f"Unsupported UUID version: {version}. Use 1 or 4.")
    return {"version": version, "uuids": results}

run(handler)
