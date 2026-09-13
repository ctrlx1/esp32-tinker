#!/usr/bin/env python3
"""Convert OurAirports large/medium airport rows into a compact C header."""

from __future__ import annotations

import csv
import io
import os
import sys
import urllib.request

SRC_URL = "https://davidmegginson.github.io/ourairports-data/airports.csv"


def to_centi(value: float, lo: int, hi: int) -> int:
    scaled = int(round(value * 100.0))
    return max(lo, min(hi, scaled))


def load_rows(path_or_url: str) -> list[dict]:
    if os.path.isfile(path_or_url):
        with open(path_or_url, "r", encoding="utf-8") as handle:
            return list(csv.DictReader(handle))
    request = urllib.request.Request(
        path_or_url, headers={"User-Agent": "ESP32-Tinker"}
    )
    with urllib.request.urlopen(request, timeout=120) as response:
        return list(csv.DictReader(io.StringIO(response.read().decode("utf-8"))))


def collect(rows: list[dict], airport_type: str) -> list[tuple[int, int]]:
    airports = []
    for row in rows:
        if row.get("type") != airport_type:
            continue
        try:
            lat = float(row["latitude_deg"])
            lon = float(row["longitude_deg"])
        except (TypeError, ValueError, KeyError):
            continue
        if not (-90.0 <= lat <= 90.0 and -180.0 <= lon <= 180.0):
            continue
        airports.append((to_centi(lat, -9000, 9000), to_centi(lon, -18000, 18000)))
    airports.sort()
    return airports


def write_array(handle, name: str, airports: list[tuple[int, int]]) -> None:
    handle.write(f"constexpr uint16_t k{name}Count = {len(airports)};\n\n")
    handle.write(f"constexpr Airport k{name}[] = {{\n")
    for lat, lon in airports:
        handle.write(f"    {{{lat}, {lon}}},\n")
    handle.write("};\n\n")


def main() -> int:
    project_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    src = sys.argv[1] if len(sys.argv) > 1 else SRC_URL
    out_path = os.path.join(project_dir, "src", "airports_data.h")

    rows = load_rows(src)
    major = collect(rows, "large_airport")
    minor = collect(rows, "medium_airport")

    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as handle:
        handle.write("#pragma once\n\n")
        handle.write("#include <stdint.h>\n\n")
        handle.write("namespace airports {\n\n")
        handle.write("struct Airport {\n  int16_t lat;\n  int16_t lon;\n};\n\n")
        write_array(handle, "Major", major)
        write_array(handle, "Minor", minor)
        handle.write("} // namespace airports\n")
    print(f"wrote {out_path} major={len(major)} minor={len(minor)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
