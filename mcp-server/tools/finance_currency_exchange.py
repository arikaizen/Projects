"""finance_currency_exchange — Currency conversion (Alpha Vantage FX)."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def handler(args, keys):
    key      = keys["ALPHA_VANTAGE_KEY"]
    from_cur = args["from_currency"].upper()
    to_cur   = args["to_currency"].upper()
    params   = urllib.parse.urlencode({"function": "CURRENCY_EXCHANGE_RATE",
                                        "from_currency": from_cur, "to_currency": to_cur,
                                        "apikey": key})
    url = f"https://www.alphavantage.co/query?{params}"
    with urllib.request.urlopen(url, timeout=15) as r:
        d = json.loads(r.read())
    r = d.get("Realtime Currency Exchange Rate", {})
    return {"from": r.get("1. From_Currency Code"), "to": r.get("3. To_Currency Code"),
            "rate": r.get("5. Exchange Rate"), "bid": r.get("8. Bid Price"),
            "ask": r.get("9. Ask Price"), "last_refreshed": r.get("6. Last Refreshed")}

run(handler)
