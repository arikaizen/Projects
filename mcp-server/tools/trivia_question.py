"""trivia_question — Random trivia question from Open Trivia DB."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json, html

def handler(args, keys):
    amount = min(int(args.get("amount", 1)), 10)
    params = {"amount": amount}
    if "category" in args:
        params["category"] = args["category"]
    if "difficulty" in args:
        params["difficulty"] = args["difficulty"]
    params["type"] = "multiple"
    url = "https://opentdb.com/api.php?" + urllib.parse.urlencode(params)
    with urllib.request.urlopen(url, timeout=15) as r:
        d = json.loads(r.read())
    if d.get("response_code") != 0:
        raise ValueError(f"OpenTDB error: response_code={d.get('response_code')}")
    questions = []
    for q in d.get("results", []):
        questions.append({
            "category": html.unescape(q.get("category", "")),
            "difficulty": q.get("difficulty"),
            "question": html.unescape(q.get("question", "")),
            "correct_answer": html.unescape(q.get("correct_answer", "")),
            "incorrect_answers": [html.unescape(a) for a in q.get("incorrect_answers", [])],
        })
    return questions

run(handler)
