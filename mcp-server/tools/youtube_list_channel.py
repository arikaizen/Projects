"""youtube_list_channel — List recent videos from a YouTube channel."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def handler(args, keys):
    key        = keys.get("YOUTUBE_API_KEY") or keys.get("GOOGLE_API_KEY","")
    channel_id = args["channel_id"]
    max_results= min(int(args.get("max_results",5)),10)
    params = urllib.parse.urlencode({"key":key,"part":"snippet","type":"video",
                                      "channelId":channel_id,"maxResults":max_results,
                                      "order":"date"})
    url = f"https://www.googleapis.com/youtube/v3/search?{params}"
    with urllib.request.urlopen(url, timeout=15) as r:
        d = json.loads(r.read())
    return [{"video_id":i["id"].get("videoId"),"title":i["snippet"]["title"],
             "published_at":i["snippet"]["publishedAt"]}
            for i in d.get("items",[]) if i["id"].get("videoId")]

run(handler)
