"""food_barcode — Look up food product by barcode via Open Food Facts."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, json

def handler(args, keys):
    barcode = str(args["barcode"]).strip()
    url = f"https://world.openfoodfacts.org/api/v0/product/{barcode}.json"
    with urllib.request.urlopen(url, timeout=15) as r:
        d = json.loads(r.read())
    if d.get("status") != 1:
        raise ValueError(f"Product not found for barcode: {barcode}")
    p = d.get("product", {})
    n = p.get("nutriments", {})
    return {
        "product_name": p.get("product_name"),
        "brands": p.get("brands"),
        "ingredients_text": p.get("ingredients_text"),
        "nutriments": {
            "energy_kcal": n.get("energy-kcal_100g"),
            "proteins": n.get("proteins_100g"),
            "fat": n.get("fat_100g"),
            "carbohydrates": n.get("carbohydrates_100g"),
        },
    }

run(handler)
