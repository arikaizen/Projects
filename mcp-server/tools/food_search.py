"""food_search — Search USDA FoodData Central for nutrition info."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

NUTRIENT_IDS = {
    "energy": [1008, 2047, 2048],
    "protein": [1003],
    "fat": [1004],
    "carbs": [1005],
}

def handler(args, keys):
    api_key = keys["USDA_API_KEY"]
    query = args["query"]
    page_size = int(args.get("page_size", 5))
    params = urllib.parse.urlencode({"query": query, "pageSize": page_size, "api_key": api_key})
    url = f"https://api.nal.usda.gov/fdc/v1/foods/search?{params}"
    with urllib.request.urlopen(url, timeout=15) as r:
        d = json.loads(r.read())
    foods = []
    for food in d.get("foods", []):
        nutrient_map = {}
        for n in food.get("foodNutrients", []):
            nid = n.get("nutrientId")
            for label, ids in NUTRIENT_IDS.items():
                if nid in ids:
                    nutrient_map[label] = {"value": n.get("value"), "unit": n.get("unitName")}
        foods.append({
            "fdcId": food.get("fdcId"),
            "description": food.get("description"),
            "brandOwner": food.get("brandOwner"),
            "nutrients": nutrient_map,
        })
    return foods

run(handler)
