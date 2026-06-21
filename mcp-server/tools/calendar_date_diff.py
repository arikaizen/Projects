"""calendar_date_diff — Calculate difference between two dates."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
from datetime import date, datetime, timedelta

def count_weekdays(start, end):
    if start > end:
        start, end = end, start
    total = 0
    current = start
    while current <= end:
        if current.weekday() < 5:
            total += 1
        current += timedelta(days=1)
    return total

def handler(args, keys):
    date_a = date.fromisoformat(args["date_a"])
    date_b_str = args.get("date_b")
    if date_b_str:
        date_b = date.fromisoformat(date_b_str)
    else:
        date_b = date.today()
    delta = date_b - date_a
    days = delta.days
    is_future = days > 0
    abs_days = abs(days)
    weeks = abs_days // 7
    months_approx = round(abs_days / 30.44, 1)
    weekdays = count_weekdays(date_a, date_b) - 1  # exclude start
    return {
        "date_a": date_a.isoformat(),
        "date_b": date_b.isoformat(),
        "days": days,
        "abs_days": abs_days,
        "weeks": weeks,
        "months_approx": months_approx,
        "is_future": is_future,
        "weekdays_between": max(weekdays, 0),
    }

run(handler)
