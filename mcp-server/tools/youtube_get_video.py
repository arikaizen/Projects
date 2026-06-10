"""youtube_get_video — Get details for a YouTube video."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def handler(args, keys):
    key   = keys.get("YOUTUBE_API_KEY") or keys.get("GOOGLE_API_KEY","")
    vid   = args["video_id"]
    params = urllib.parse.urlencode({"key": key, "part":"snippet,statistics,contentDetails",
                                      "id": vid})
    url = f"https://www.googleapis.com/youtube/v3/videos?{params}"
    with urllib.request.urlopen(url, timeout=15) as r:
        d = json.loads(r.read())
    if not d.get("items"): raise ValueError("Video not found")
    item = d["items"][0]
    s = item["snippet"]; st = item["statistics"]; cd = item["contentDetails"]
    return {"title":s["title"],"channel":s["channelTitle"],"published_at":s["publishedAt"],
            "description":s["description"][:500],"duration":cd["duration"],
            "views":st.get("viewCount"),"likes":st.get("likeCount"),
            "comments":st.get("commentCount")}

run(handler)
