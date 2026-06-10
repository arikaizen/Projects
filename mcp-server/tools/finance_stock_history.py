"""finance_stock_history — Daily adjusted stock prices (Alpha Vantage)."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def handler(args, keys):
    key      = keys["ALPHA_VANTAGE_KEY"]
    symbol   = args["symbol"].upper()
    outputsz = args.get("output_size", "compact")  # compact=100 days, full=20 years
    params   = urllib.parse.urlencode({"function": "TIME_SERIES_DAILY_ADJUSTED",
                                        "symbol": symbol, "outputsize": outputsz, "apikey": key})
    url = f"https://www.alphavantage.co/query?{params}"
    with urllib.request.urlopen(url, timeout=20) as r:
        d = json.loads(r.read())
    series = d.get("Time Series (Daily)", {})
    days   = list(series.items())[:int(args.get("days", 10))]
    return [{"date": k, "open": v["1. open"], "high": v["2. high"],
             "low": v["3. low"], "close": v["4. close"],
             "volume": v["6. volume"]} for k, v in days]

run(handler)
