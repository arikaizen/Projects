"""google_vision_labels — Detect labels in an image (Google Vision API)."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, json, base64

def handler(args, keys):
    key      = keys["GOOGLE_API_KEY"]
    image_url = args.get("image_url","")
    if image_url:
        image_field = {"source": {"imageUri": image_url}}
    else:
        b64 = args.get("image_base64","")
        if not b64: raise ValueError("Provide image_url or image_base64")
        image_field = {"content": b64}
    body = json.dumps({"requests":[{"image": image_field,
                                     "features":[{"type":"LABEL_DETECTION","maxResults":10}]}]}).encode()
    url  = f"https://vision.googleapis.com/v1/images:annotate?key={key}"
    req  = urllib.request.Request(url, data=body, method="POST",
                                   headers={"Content-Type":"application/json"})
    with urllib.request.urlopen(req, timeout=20) as r:
        d = json.loads(r.read())
    labels = d["responses"][0].get("labelAnnotations",[])
    return [{"label":l["description"],"score":round(l["score"],3)} for l in labels]

run(handler)
