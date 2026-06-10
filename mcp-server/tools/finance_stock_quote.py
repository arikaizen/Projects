"""finance_stock_quote — Real-time stock quote (Alpha Vantage)."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def handler(args, keys):
    key    = keys["ALPHA_VANTAGE_KEY"]
    symbol = args["symbol"].upper()
    params = urllib.parse.urlencode({"function": "GLOBAL_QUOTE", "symbol": symbol, "apikey": key})
    url    = f"https://www.alphavantage.co/query?{params}"
    with urllib.request.urlopen(url, timeout=15) as r:
        d = json.loads(r.read())
    q = d.get("Global Quote", {})
    return {"symbol": q.get("01. symbol"), "price": q.get("05. price"),
            "change": q.get("09. change"), "change_percent": q.get("10. change percent"),
            "volume": q.get("06. volume"), "latest_trading_day": q.get("07. latest trading day")}

run(handler)
