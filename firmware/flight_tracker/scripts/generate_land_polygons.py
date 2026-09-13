#!/usr/bin/env python3
"""Convert Natural Earth 110m land/lakes GeoJSON into a compact C header."""

from __future__ import annotations

import json
import os
import sys
import urllib.request

LAND_URL = (
    "https://raw.githubusercontent.com/nvkelso/natural-earth-vector/"
    "master/geojson/ne_110m_land.geojson"
)
LAKES_URL = (
    "https://raw.githubusercontent.com/nvkelso/natural-earth-vector/"
    "master/geojson/ne_110m_lakes.geojson"
)

FLAG_HOLE = 1
FLAG_WATER = 2


def to_centi(value: float) -> int:
    scaled = int(round(value * 100.0))
    return max(-18000, min(18000, scaled))


def load_geojson(path_or_url: str) -> dict:
    if os.path.isfile(path_or_url):
        with open(path_or_url, "r", encoding="utf-8") as handle:
            return json.load(handle)
    request = urllib.request.Request(path_or_url, headers={"User-Agent": "ESP32-Tinker"})
    with urllib.request.urlopen(request, timeout=60) as response:
        return json.loads(response.read().decode("utf-8"))


def add_rings(data: dict, water: bool, verts: list, rings: list) -> None:
    group = 0 if not rings else rings[-1][6] + 1
    for feature in data["features"]:
        geometry = feature["geometry"]
        polys = (
            geometry["coordinates"]
            if geometry["type"] == "MultiPolygon"
            else [geometry["coordinates"]]
        )
        for poly in polys:
            for index, ring in enumerate(poly):
                start = len(verts)
                for lon, lat in ring:
                    verts.append((to_centi(lat), to_centi(lon)))
                count = len(verts) - start
                if count < 3:
                    verts[start:] = []
                    continue
                lats = [verts[i][0] for i in range(start, start + count)]
                lons = [verts[i][1] for i in range(start, start + count)]
                flags = FLAG_WATER if water else 0
                if index > 0:
                    flags |= FLAG_HOLE
                rings.append(
                    (
                        min(lats),
                        max(lats),
                        min(lons),
                        max(lons),
                        start,
                        count,
                        group,
                        flags,
                    )
                )
            group += 1


def write_header(path: str, verts: list, rings: list) -> None:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as handle:
        handle.write("#pragma once\n\n")
        handle.write("#include <stdint.h>\n\n")
        handle.write("namespace land_polygons {\n\n")
        handle.write("constexpr uint8_t kFlagHole = 1;\n")
        handle.write("constexpr uint8_t kFlagWater = 2;\n\n")
        handle.write("struct Vert {\n  int16_t lat;\n  int16_t lon;\n};\n\n")
        handle.write("struct Ring {\n")
        handle.write("  int16_t minLat;\n  int16_t maxLat;\n")
        handle.write("  int16_t minLon;\n  int16_t maxLon;\n")
        handle.write("  uint16_t start;\n  uint16_t count;\n")
        handle.write("  uint16_t group;\n  uint8_t flags;\n};\n\n")
        handle.write(f"constexpr uint16_t kVertCount = {len(verts)};\n")
        handle.write(f"constexpr uint16_t kRingCount = {len(rings)};\n\n")
        handle.write("constexpr Vert kVerts[] = {\n")
        for lat, lon in verts:
            handle.write(f"    {{{lat}, {lon}}},\n")
        handle.write("};\n\n")
        handle.write("constexpr Ring kRings[] = {\n")
        for ring in rings:
            handle.write(
                "    {{{}, {}, {}, {}, {}, {}, {}, {}}},\n".format(*ring)
            )
        handle.write("};\n\n")
        handle.write("} // namespace land_polygons\n")


def main() -> int:
    project_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    land_src = sys.argv[1] if len(sys.argv) > 1 else LAND_URL
    lakes_src = sys.argv[2] if len(sys.argv) > 2 else LAKES_URL
    out_path = os.path.join(project_dir, "src", "land_polygons_data.h")

    verts: list = []
    rings: list = []
    add_rings(load_geojson(land_src), False, verts, rings)
    add_rings(load_geojson(lakes_src), True, verts, rings)
    write_header(out_path, verts, rings)
    print(f"wrote {out_path} verts={len(verts)} rings={len(rings)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
