"""Golden records for the Arabic date and time functions arbtok calls.

The gold comes from the Python reference ovos-date-parser, at the version
arbtok 0.0.2a71 resolves to. The record format is the one of
tools/goldens/numbers_ar.py: [kind, arguments..., result], and a "_block"
record holds the SHA-256 of the results of a range.

The inputs are the recorded calls in tests/goldens/numbers_calls.jsonl.gz
(written by numbers_ar.py record), every hour and minute of the day in each
nice_time flag, every day of the years 1 to 9999 for nice_date, the texts
arbtok's text normalizer gives extract_datetime ("الساعة <hour> <word>" and
"<word>"), and random sequences of the date vocabulary at several anchors.

A text that names a holiday reaches chronologia, which is not ported. The
port answers such a text from data/numbers/holidays_ar.json for lang "ar" at
the anchor 2000-01-01 only, so a generated text that names a holiday and has
no answer there is left out, and counted.

Usage, with the reference interpreter:
    python -I tools/goldens/dates_ar.py tests/goldens/numbers_calls.jsonl.gz \\
        data/numbers/holidays_ar.json tests/goldens/numbers_dates_ar.jsonl.gz [--jobs N]
"""
import datetime
import gzip
import hashlib
import json
import multiprocessing
import os
import pathlib
import random
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent))

from goldens import common  # noqa: E402

ANCHOR = datetime.datetime(2000, 1, 1)
ANCHORS = [ANCHOR, datetime.datetime(2024, 2, 29, 13, 45, 10, 500000), datetime.datetime(1, 1, 1),
           datetime.datetime(9999, 12, 31, 23, 59), datetime.datetime(2023, 12, 31, 0, 0, 1),
           datetime.datetime(1999, 3, 1, 12)]


def dumps(value):
    return json.dumps(value, ensure_ascii=False, separators=(",", ":"))


def fields(dt):
    """[year, month, day] of a date, with [hour, minute, second, microsecond] after
    it for a datetime; a time is a datetime on 0001-01-01."""
    if isinstance(dt, datetime.time):
        return [1, 1, 1, dt.hour, dt.minute, dt.second, dt.microsecond]
    out = [dt.year, dt.month, dt.day]
    if isinstance(dt, datetime.datetime):
        out += [dt.hour, dt.minute, dt.second, dt.microsecond]
    return out


def as_datetime(value):
    if isinstance(value, dict):
        if "datetime" in value:
            return datetime.datetime.fromisoformat(value["datetime"])
        if "date" in value:
            return datetime.date.fromisoformat(value["date"])
        if "time" in value:
            return datetime.time.fromisoformat(value["time"])
    return value


def enc(fn):
    try:
        return fn()
    except Exception as e:  # the class of the error is part of the gold
        return {"err": type(e).__name__}


def digest(results):
    return hashlib.sha256("\n".join(dumps(r) for r in results).encode("utf-8")).hexdigest()


def r_nice_time(dt, lang, speech, use_24hour, use_ampm):
    from ovos_date_parser import nice_time
    return ["nice_time", fields(dt), lang, speech, use_24hour, use_ampm,
            enc(lambda: nice_time(dt, lang, speech=speech, use_24hour=use_24hour, use_ampm=use_ampm))]


def r_nice_date(dt, lang):
    from ovos_date_parser import nice_date
    return ["nice_date", fields(dt)[:3], lang, enc(lambda: nice_date(dt, lang))]


def r_extract_datetime(text, lang, anchor):
    from ovos_date_parser import extract_datetime

    def run():
        found = extract_datetime(text, lang, anchor)
        return None if found is None else [found[0].isoformat(), found[1]]
    return ["extract_datetime", text, lang, fields(anchor), enc(run)]


def nice_date_block(first, last, lang):
    from ovos_date_parser import nice_date
    results = []
    day = datetime.date(first, 1, 1)
    while day.year <= last:
        results.append(enc(lambda: nice_date(day, lang)))
        if day == datetime.date.max:
            break
        day += datetime.timedelta(days=1)
    return ["nice_date_block", first, last, lang, digest(results)]


def _call_star(args):
    fn, a = args
    return globals()[fn](*a)


def from_recorded(calls_path):
    tasks = []
    n_in = 0
    with gzip.open(calls_path, "rt", encoding="utf-8") as f:
        for line in f:
            name, args, kwargs = json.loads(line)
            if name not in ("nice_time", "nice_date", "extract_datetime"):
                continue
            n_in += 1
            args = [as_datetime(a) for a in args]
            if name == "nice_time":
                tasks.append(("r_nice_time", (args[0], args[1], kwargs.get("speech", True),
                                              kwargs.get("use_24hour", False), kwargs.get("use_ampm", False))))
            elif name == "nice_date":
                assert not kwargs, kwargs
                tasks.append(("r_nice_date", (args[0], args[1])))
            else:
                assert not kwargs and len(args) == 3, (args, kwargs)
                tasks.append(("r_extract_datetime", (args[0], args[1], args[2])))
    return n_in, tasks


def date_vocabulary():
    import ovos_date_parser.dates_ar as da
    words = set()
    for name in ("_UNITS_LOOKUP_AR", "_DUALS_LOOKUP_AR", "_MONTHS_LOOKUP_AR", "_WEEKDAYS_LOOKUP_AR",
                 "_HOUR_LOOKUP_AR", "_MINUTE_WORDS_AR", "_TODAY_WORDS", "_TOMORROW_WORDS", "_YESTERDAY_WORDS",
                 "_AM_WORDS", "_PM_WORDS", "_NOON_WORDS", "_NEXT_WORDS", "_PREV_WORDS", "_AFTER_WORDS",
                 "_BEFORE_WORDS", "_CLOCK_WORDS", "_CALENDAR_UNITS", "_QUARTER_WORDS", "_THIRD_WORDS",
                 "_HALF_WORDS"):
        words.update(getattr(da, name))
    for table in (da._MONTHS_AR, da._WEEKDAYS_AR, da._HOUR_NAMES_AR, da._UNIT_SECONDS_AR,
                  da._DUAL_UNIT_SECONDS_AR):
        words.update(table.values() if isinstance(next(iter(table)), int) else table.keys())
    words.update([da._MIDNIGHT_WORD, da._EXCEPT_WORD, da._DAY_WORD, da._OF_WORD, da._IN_WORD, "و", "منتصف",
                  "الليل", "بعد", "غد", "اول", "امس", "الأول", "الظهر", "غضون", "في", "عشرة", "عشره", "الحادية",
                  "الثانية", "خمس", "ثلاث", "عشرين", "خمسة", "واحد", "نصف", "ربع", "ثلث", "الخامس", "العشرين",
                  "رمضان", "العيد", "عيد", "الفطر", "الأضحى", "المولد", "عاشوراء", "نوروز", "حانوكا", "الميلاد",
                  "سيارة", "موعد", "عندي", "تذكير", "اجتماع"])
    return sorted(w for w in words if w)


TOKENS = ["5", "12", "0", "24", "25", "3:15", "23:59", "24:00", "5:70", "١٠:٣٠", "1.5", "2024", "999", "10000",
          "31", "32", "1e9", "-3"]


def holiday_free(text, lang, anchor, answers):
    """Whether the port answers text: no holiday named, or the reference answer is in its table."""
    from ovos_date_parser.holidays import _names_a_holiday
    if not _names_a_holiday(text, lang):
        return True
    return lang == "ar" and anchor == ANCHOR and text in answers


def generated_tasks(answers):
    tasks, skipped = [], 0
    for lang in ("ar", "ar-SA", "arz"):
        for hour in range(24):
            for minute in range(60):
                for speech in (True, False):
                    for use_24hour in (False, True):
                        for use_ampm in (False, True):
                            dt = datetime.datetime(2000, 1, 1, hour, minute)
                            if lang != "ar" and not speech:
                                continue
                            tasks.append(("r_nice_time", (dt, lang, speech, use_24hour, use_ampm)))
    for hour, minute in ((0, 0), (12, 0), (13, 40), (23, 45)):
        tasks.append(("r_nice_time", (datetime.time(hour, minute), "ar", True, True, False)))
    for lang in ("ar", "AR-eg", "ar_SA", "arz"):
        for day in (datetime.date(1, 1, 1), datetime.date(2000, 2, 29), datetime.date(9999, 12, 31),
                    datetime.date(1987, 6, 15)):
            tasks.append(("r_nice_date", (day, lang)))
    vocab = date_vocabulary()
    import ovos_number_parser.numbers_ar as na
    words = vocab + sorted(set(na._ONES_AR + na._ONES_FEM_AR) - {""})
    # the texts the text normalizer sends: "الساعة <hour> <word>" and "<word>"
    for word in words:
        if not all("ء" <= c <= "ي" for c in word):
            continue
        for text in [word] + [f"الساعة {hour} {word}" for hour in range(1, 13)]:
            if holiday_free(text, "ar", ANCHOR, answers):
                tasks.append(("r_extract_datetime", (text, "ar", ANCHOR)))
            else:
                skipped += 1
    for text in answers:
        tasks.append(("r_extract_datetime", (text, "ar", ANCHOR)))
    rng = random.Random(5)
    pool = words + TOKENS * 3
    for _ in range(30000):
        text = " ".join(rng.choice(pool) for _ in range(rng.randint(1, 7)))
        lang = rng.choice(["ar", "ar", "ar", "ar-SA"])
        anchor = rng.choice(ANCHORS)
        if holiday_free(text, lang, anchor, answers):
            tasks.append(("r_extract_datetime", (text, lang, anchor)))
        else:
            skipped += 1
    for text in ["", " ", "غدا", "بعد 1e9 سنة", "قبل 999999999 يوم", "بعد 3000000 يوم", "بعد ساعة ونصف",
                 "بعد ثلث ساعة", "الساعة الثالثة إلا ربعا", "الساعة الواحدة إلا خمس دقائق", "5 من مارس 2024",
                 "الخامس من فبراير", "30 فبراير", "يوم الجمعة القادم", "الأسبوع الماضي", "الساعة 25"]:
        for anchor in ANCHORS:
            if holiday_free(text, "ar", anchor, answers):
                tasks.append(("r_extract_datetime", (text, "ar", anchor)))
            else:
                skipped += 1
    return tasks, skipped


def main():
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    calls_path, holidays_path, out_path = sys.argv[1:4]
    jobs = int(sys.argv[sys.argv.index("--jobs") + 1]) if "--jobs" in sys.argv else os.cpu_count()
    with open(holidays_path, encoding="utf-8") as f:
        answers = json.load(f)["answers"]
    n_in, recorded = from_recorded(calls_path)
    generated, skipped = generated_tasks(answers)
    blocks = [(first, min(first + 9, 9999), "ar") for first in range(1, 10000, 10)]
    n_tasks = len(recorded) + len(generated) + len(blocks)
    with multiprocessing.Pool(jobs) as pool:
        records = pool.map(_call_star, recorded + generated, chunksize=100)
        records += pool.starmap(nice_date_block, blocks, chunksize=2)
    records, duplicates = common.deduplicated(records)
    print(f"recorded date calls in: {n_in}; out: {len(recorded)}", file=sys.stderr)
    print(f"generated in: {len(generated) + skipped}; out: {len(generated)}; left out as holiday texts "
          f"the port has no answer for: {skipped}", file=sys.stderr)
    print(f"blocks in: {len(blocks)}; tasks in: {n_tasks}; repeats: {duplicates}; records out: {len(records)}",
          file=sys.stderr)
    common.write(out_path, "numbers_dates_ar", [("dates", n_tasks - duplicates, records)])


if __name__ == "__main__":
    main()
