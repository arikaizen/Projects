"""finance_company_info — Company overview (Alpha Vantage)."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def handler(args, keys):
    key    = keys["ALPHA_VANTAGE_KEY"]
    symbol = args["symbol"].upper()
    params = urllib.parse.urlencode({"function": "OVERVIEW", "symbol": symbol, "apikey": key})
    url    = f"https://www.alphavantage.co/query?{params}"
    with urllib.request.urlopen(url, timeout=15) as r:
        d = json.loads(r.read())
    return {k: d.get(k) for k in ["Symbol","Name","Description","Exchange","Currency","Country",
                                    "Sector","Industry","MarketCapitalization","PERatio",
                                    "52WeekHigh","52WeekLow","DividendYield","EPS"]}

run(handler)
