"""youtube_search — Search YouTube videos."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def handler(args, keys):
    key    = keys["YOUTUBE_API_KEY"] if "YOUTUBE_API_KEY" in keys else keys.get("GOOGLE_API_KEY","")
    params = urllib.parse.urlencode({"key": key, "part":"snippet", "type":"video",
                                      "q": args["query"],
                                      "maxResults": min(int(args.get("max_results",5)),10),
                                      "order": args.get("order","relevance")})
    url = f"https://www.googleapis.com/youtube/v3/search?{params}"
    with urllib.request.urlopen(url, timeout=15) as r:
        d = json.loads(r.read())
    return [{"video_id":i["id"]["videoId"],"title":i["snippet"]["title"],
             "channel":i["snippet"]["channelTitle"],"published_at":i["snippet"]["publishedAt"],
             "description":i["snippet"]["description"][:200],
             "url":f"https://youtube.com/watch?v={i['id']['videoId']}"}
            for i in d.get("items",[]) if i["id"].get("videoId")]

run(handler)
