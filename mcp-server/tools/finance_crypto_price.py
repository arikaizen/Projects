"""finance_crypto_price — Cryptocurrency price (Alpha Vantage)."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def handler(args, keys):
    key    = keys["ALPHA_VANTAGE_KEY"]
    symbol = args["symbol"].upper()  # e.g. BTC
    market = args.get("market", "USD")
    params = urllib.parse.urlencode({"function": "CURRENCY_EXCHANGE_RATE",
                                      "from_currency": symbol, "to_currency": market,
                                      "apikey": key})
    url = f"https://www.alphavantage.co/query?{params}"
    with urllib.request.urlopen(url, timeout=15) as r:
        d = json.loads(r.read())
    r = d.get("Realtime Currency Exchange Rate", {})
    return {"symbol": symbol, "market": market, "price": r.get("5. Exchange Rate"),
            "last_refreshed": r.get("6. Last Refreshed")}

run(handler)
