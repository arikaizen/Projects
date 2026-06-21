"""math_evaluate — Safely evaluate a math expression."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import ast, math, operator

SAFE_FUNCS = {
    "sqrt": math.sqrt, "sin": math.sin, "cos": math.cos, "tan": math.tan,
    "log": math.log, "abs": abs, "floor": math.floor, "ceil": math.ceil,
    "round": round, "pi": math.pi, "e": math.e,
}

SAFE_OPS = {
    ast.Add: operator.add, ast.Sub: operator.sub, ast.Mult: operator.mul,
    ast.Div: operator.truediv, ast.Pow: operator.pow, ast.Mod: operator.mod,
    ast.USub: operator.neg, ast.UAdd: operator.pos,
}

def _eval(node):
    if isinstance(node, ast.Constant) and isinstance(node.value, (int, float)):
        return node.value
    if isinstance(node, ast.Name) and node.id in SAFE_FUNCS:
        return SAFE_FUNCS[node.id]
    if isinstance(node, ast.BinOp) and type(node.op) in SAFE_OPS:
        return SAFE_OPS[type(node.op)](_eval(node.left), _eval(node.right))
    if isinstance(node, ast.UnaryOp) and type(node.op) in SAFE_OPS:
        return SAFE_OPS[type(node.op)](_eval(node.operand))
    if isinstance(node, ast.Call):
        func = _eval(node.func)
        if callable(func):
            call_args = [_eval(a) for a in node.args]
            return func(*call_args)
    raise ValueError(f"Unsupported expression node: {type(node).__name__}")

def handler(args, keys):
    expression = args["expression"]
    try:
        tree = ast.parse(expression, mode="eval")
        result = _eval(tree.body)
        return {"expression": expression, "result": result}
    except Exception as exc:
        raise ValueError(f"Cannot evaluate expression: {exc}") from exc

run(handler)
