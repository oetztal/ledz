#!/usr/bin/env python3
"""Generate the timezone <option> rows for data/timers.html.

The POSIX TZ string the device stores is read straight out of the system
tzdata: the last line of a TZif v2+ file is its POSIX footer, which is
exactly the grammar newlib's tzset() parses.

    $ awk 'END{print}' /usr/share/zoneinfo/Europe/Berlin
    CET-1CEST,M3.5.0,M10.5.0/3

Never hand-type these strings — several are counterintuitive (Brazil
abolished DST in 2019, Mexico in 2022, Iran in 2022, Egypt reinstated it
in 2023) and the POSIX offset sign is inverted relative to UTC offsets.

Usage:
    python3 scripts/gen_timezones.py            # print the <option> rows
    python3 scripts/gen_timezones.py --json     # machine-readable, for tests
"""

import argparse
import datetime
import json
import os
import sys
import zoneinfo

ZONEINFO_DIR = "/usr/share/zoneinfo"

# Zones are labelled by cities, not by offset — the user picks a place.
# The list covers every distinct DST rule with meaningful population.
#
# The first IANA name of each entry supplies the POSIX string; the rest are
# aliases that the page's Detect button also matches. Zones sharing a rule
# share a row — two <option>s with the same value cannot be told apart on
# reload, since select.value would always pick the first.
ALIASES = {
    "America/Sao_Paulo": ["America/Argentina/Buenos_Aires", "America/Montevideo"],
    "Asia/Shanghai": ["Asia/Singapore", "Australia/Perth", "Asia/Hong_Kong", "Asia/Taipei"],
    "Asia/Tokyo": ["Asia/Seoul"],
    "Europe/Berlin": [
        "Europe/Paris", "Europe/Madrid", "Europe/Rome", "Europe/Amsterdam",
        "Europe/Brussels", "Europe/Vienna", "Europe/Prague", "Europe/Warsaw",
        "Europe/Stockholm", "Europe/Oslo", "Europe/Copenhagen", "Europe/Zurich",
        "Europe/Budapest",
    ],
    "Europe/London": ["Europe/Dublin", "Europe/Lisbon"],
    "Europe/Athens": ["Europe/Helsinki", "Europe/Kyiv", "Europe/Bucharest", "Europe/Riga"],
    "Europe/Moscow": ["Europe/Istanbul", "Africa/Nairobi", "Asia/Riyadh"],
    "America/New_York": ["America/Toronto", "America/Detroit"],
    "America/Chicago": ["America/Winnipeg"],
    "America/Mexico_City": ["America/Guatemala", "America/Costa_Rica"],
    "America/Bogota": ["America/Lima", "America/Panama"],
    "Asia/Karachi": ["Asia/Tashkent"],
    "Asia/Dhaka": ["Asia/Almaty"],
    "Pacific/Noumea": ["Pacific/Guadalcanal"],
    "Pacific/Apia": ["Pacific/Tongatapu"],
    "Pacific/Midway": ["Pacific/Pago_Pago"],
    "America/Denver": ["America/Edmonton"],
    "America/Los_Angeles": ["America/Vancouver", "America/Tijuana"],
    "Asia/Kolkata": ["Asia/Colombo"],
    "Asia/Bangkok": ["Asia/Jakarta", "Asia/Ho_Chi_Minh"],
    "Asia/Dubai": ["Asia/Baku", "Asia/Tbilisi", "Asia/Muscat"],
    "Australia/Sydney": ["Australia/Melbourne", "Australia/Canberra", "Australia/Hobart"],
    "Pacific/Auckland": ["Pacific/Fiji"],
    "Africa/Lagos": ["Africa/Algiers", "Africa/Kinshasa"],
    "Africa/Johannesburg": ["Africa/Harare", "Africa/Maputo"],
}

ZONES = [
    ("Pacific/Midway", "Midway, Pago Pago"),
    ("Pacific/Honolulu", "Honolulu, Hawaii"),
    ("Pacific/Marquesas", "Marquesas Islands"),
    ("America/Anchorage", "Anchorage, Alaska"),
    ("America/Los_Angeles", "Los Angeles, Vancouver, Seattle"),
    ("America/Phoenix", "Phoenix, Arizona (no DST)"),
    ("America/Denver", "Denver, Calgary"),
    ("America/Mexico_City", "Mexico City, Guatemala (no DST)"),
    ("America/Chicago", "Chicago, Winnipeg"),
    ("America/Bogota", "Bogota, Lima, Panama"),
    ("America/New_York", "New York, Toronto, Miami"),
    ("America/Caracas", "Caracas"),
    ("America/Halifax", "Halifax, Bermuda"),
    ("America/Santiago", "Santiago"),
    ("America/St_Johns", "St. John's, Newfoundland"),
    ("America/Sao_Paulo", "Sao Paulo, Buenos Aires, Montevideo (no DST)"),
    ("Atlantic/South_Georgia", "South Georgia"),
    ("Atlantic/Azores", "Azores"),
    ("Atlantic/Cape_Verde", "Cape Verde"),
    ("UTC", "UTC (no DST)"),
    ("Europe/London", "London, Dublin, Lisbon"),
    ("Africa/Lagos", "Lagos, Algiers, Kinshasa"),
    ("Europe/Berlin", "Berlin, Paris, Madrid, Rome"),
    ("Africa/Johannesburg", "Johannesburg, Harare, Maputo"),
    ("Africa/Cairo", "Cairo"),
    ("Europe/Athens", "Athens, Helsinki, Kyiv, Bucharest"),
    ("Asia/Jerusalem", "Jerusalem"),
    ("Europe/Moscow", "Moscow, Istanbul, Nairobi"),
    ("Asia/Tehran", "Tehran (no DST)"),
    ("Asia/Dubai", "Dubai, Baku, Tbilisi"),
    ("Asia/Kabul", "Kabul"),
    ("Asia/Karachi", "Karachi, Tashkent"),
    ("Asia/Kolkata", "India, Sri Lanka"),
    ("Asia/Kathmandu", "Kathmandu"),
    ("Asia/Dhaka", "Dhaka, Almaty"),
    ("Asia/Yangon", "Yangon"),
    ("Asia/Bangkok", "Bangkok, Jakarta, Hanoi"),
    ("Asia/Shanghai", "Beijing, Shanghai, Singapore, Perth"),
    ("Australia/Eucla", "Eucla"),
    ("Asia/Tokyo", "Tokyo, Seoul"),
    ("Australia/Darwin", "Darwin"),
    ("Australia/Brisbane", "Brisbane (no DST)"),
    ("Australia/Adelaide", "Adelaide"),
    ("Australia/Sydney", "Sydney, Melbourne, Canberra"),
    ("Pacific/Noumea", "Noumea, Solomon Islands"),
    ("Pacific/Auckland", "Auckland, Wellington, Fiji"),
    ("Pacific/Chatham", "Chatham Islands"),
    ("Pacific/Apia", "Apia, Tongatapu"),
    ("Pacific/Kiritimati", "Kiritimati"),
]


def posix_footer(zone):
    """Last line of the TZif v2+ file — the POSIX TZ string for the zone."""
    path = os.path.join(ZONEINFO_DIR, zone)
    with open(path, "rb") as handle:
        lines = handle.read().split(b"\n")
    for line in reversed(lines):
        if line.strip():
            return line.decode("ascii")
    raise ValueError(f"{zone}: no POSIX footer (TZif v1 file?)")


def standard_offset_minutes(zone):
    """Current standard (non-DST) offset, used only for sorting the list."""
    tz = zoneinfo.ZoneInfo(zone)
    offsets = []
    for month in (1, 7):
        moment = datetime.datetime(2026, month, 15, 12, 0, tzinfo=tz)
        offsets.append(moment.utcoffset() - moment.dst())
    return min(offsets).total_seconds() / 60


def collect():
    rows = []
    for zone, label in ZONES:
        tz_string = posix_footer(zone)
        if len(tz_string) > 63:
            raise ValueError(f"{zone}: POSIX string exceeds 63 chars: {tz_string}")
        rows.append(
            {
                "iana": zone,
                "aliases": ALIASES.get(zone, []),
                "label": label,
                "tz": tz_string,
                "std_offset_minutes": standard_offset_minutes(zone),
            }
        )
    rows.sort(key=lambda row: (row["std_offset_minutes"], row["label"]))

    seen = {}
    for row in rows:
        if row["tz"] in seen:
            raise ValueError(
                f'{row["iana"]} and {seen[row["tz"]]} share the POSIX string '
                f'{row["tz"]} — merge them into one row with an alias'
            )
        seen[row["tz"]] = row["iana"]

    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--json", action="store_true", help="emit JSON instead of HTML")
    args = parser.parse_args()

    rows = collect()

    if args.json:
        json.dump(rows, sys.stdout, indent=2)
        sys.stdout.write("\n")
        return

    width = max(len(row["tz"]) for row in rows)
    for row in rows:
        value = f'"{row["tz"]}"'.ljust(width + 2)
        iana = " ".join([row["iana"]] + row["aliases"])
        print(f'<option value={value} data-iana="{iana}">{row["label"]}</option>')


if __name__ == "__main__":
    main()
