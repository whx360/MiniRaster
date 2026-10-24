#!/usr/bin/env python3
"""Summarize measured run_*.csv files using only Python's standard library.

Each row must have exact_match=1. Timings are never inferred or substituted.
The fixed comparison uses Tile=32 and four workers when all three modes exist.
"""

import argparse
import csv
import io
import json
import math
import os
import statistics
import sys
import tempfile
from collections import defaultdict
from pathlib import Path


REQUIRED_COLUMNS = {
    "scene", "mode", "tile", "threads", "frame_ms", "geometry_ms",
    "bin_ms", "raster_ms", "auxiliary_bytes", "framebuffer_bytes", "exact_match",
}
SUMMARY_COLUMNS = (
    "scene", "mode", "tile", "threads", "n", "median_ms", "mean_ms",
    "stdev_ms", "p95_ms", "geometry_median_ms", "bin_median_ms",
    "raster_median_ms", "auxiliary_bytes", "framebuffer_bytes",
)


def percentile(values, quantile):
    """Linear interpolation between adjacent ordered observations."""
    ordered = sorted(values)
    position = (len(ordered) - 1) * quantile
    lower = int(position)
    upper = min(lower + 1, len(ordered) - 1)
    return ordered[lower] + (ordered[upper] - ordered[lower]) * (position - lower)


def integer(value, label, minimum=0):
    if not value or not value.isascii() or not value.isdigit():
        raise ValueError(f"{label} must be an integer >= {minimum}")
    result = int(value)
    if result < minimum:
        raise ValueError(f"{label} must be >= {minimum}")
    return result


def timing(value, label, strictly_positive=False):
    try:
        result = float(value)
    except (TypeError, ValueError) as error:
        raise ValueError(f"{label} must be a finite number") from error
    if not math.isfinite(result) or result < 0 or (strictly_positive and result == 0):
        relation = "> 0" if strictly_positive else ">= 0"
        raise ValueError(f"{label} must be finite and {relation}")
    return result


def read_runs(input_dir):
    if not input_dir.is_dir():
        raise ValueError(f"input directory does not exist: {input_dir}")
    files = sorted(input_dir.glob("run_*.csv"))
    if not files:
        raise ValueError(f"no run_*.csv files found in {input_dir}")
    groups = defaultdict(list)
    dimensions = set()
    dimension_columns = None
    for path in files:
        with path.open(encoding="utf-8-sig", newline="") as stream:
            reader = csv.DictReader(stream)
            header = reader.fieldnames or []
            missing = REQUIRED_COLUMNS.difference(header)
            if missing:
                raise ValueError(f"{path.name}: missing columns: {', '.join(sorted(missing))}")
            if len(header) != len(set(header)):
                raise ValueError(f"{path.name}: duplicate column names")
            has_dimensions = "width" in header and "height" in header
            if ("width" in header) != ("height" in header):
                raise ValueError(f"{path.name}: width and height must appear together")
            if dimension_columns is None:
                dimension_columns = has_dimensions
            elif dimension_columns != has_dimensions:
                raise ValueError("input files must use consistent width/height columns")
            count = 0
            for row in reader:
                location = f"{path.name}:{reader.line_num}"
                try:
                    if None in row or any(value is None for value in row.values()):
                        raise ValueError("CSV row has the wrong number of fields")
                    if row["exact_match"] != "1":
                        raise ValueError("exact_match must be 1 for every measured row")
                    scene, mode = row["scene"], row["mode"]
                    if not scene or scene != scene.strip():
                        raise ValueError("scene must be a nonempty name without outer whitespace")
                    if mode not in {"reference", "tile", "parallel"}:
                        raise ValueError(f"unknown render mode: {mode}")
                    tile = integer(row["tile"], "tile", 1)
                    threads = integer(row["threads"], "threads", 1)
                    if has_dimensions:
                        dimensions.add((integer(row["width"], "width", 1),
                                        integer(row["height"], "height", 1)))
                        if len(dimensions) != 1:
                            raise ValueError("mixed resolutions must be analyzed in separate directories")
                    measured = {
                        name: timing(row[name], name, strictly_positive=name == "frame_ms")
                        for name in ("frame_ms", "geometry_ms", "bin_ms", "raster_ms")
                    }
                    measured["auxiliary_bytes"] = integer(row["auxiliary_bytes"], "auxiliary_bytes")
                    measured["framebuffer_bytes"] = integer(row["framebuffer_bytes"], "framebuffer_bytes", 1)
                    groups[scene, mode, tile, threads].append(measured)
                    count += 1
                except ValueError as error:
                    raise ValueError(f"{location}: {error}") from error
            if count == 0:
                raise ValueError(f"{path.name}: contains no measured rows")
    return files, groups


def summarize(groups):
    records = []
    lookup = {}
    for key, rows in sorted(groups.items()):
        framebuffers = {row["framebuffer_bytes"] for row in rows}
        if len(framebuffers) != 1:
            raise ValueError(f"{key}: inconsistent framebuffer_bytes")
        times = [row["frame_ms"] for row in rows]
        record = dict(zip(("scene", "mode", "tile", "threads"), key))
        record.update(
            n=len(times), median_ms=statistics.median(times),
            mean_ms=statistics.mean(times),
            stdev_ms=statistics.stdev(times) if len(times) > 1 else 0.0,
            p95_ms=percentile(times, 0.95),
            geometry_median_ms=statistics.median(row["geometry_ms"] for row in rows),
            bin_median_ms=statistics.median(row["bin_ms"] for row in rows),
            raster_median_ms=statistics.median(row["raster_ms"] for row in rows),
            auxiliary_bytes=max(row["auxiliary_bytes"] for row in rows),
            framebuffer_bytes=next(iter(framebuffers)),
        )
        records.append(record)
        lookup[key] = record
    fixed = {}
    skipped = []
    for scene in sorted({key[0] for key in groups}):
        # Reference tile is irrelevant to traversal. Accept any unambiguous
        # one-worker reference configuration, rather than assuming tile=16.
        references = [record for key, record in lookup.items()
                      if key[0] == scene and key[1] == "reference" and key[3] == 1]
        tile = lookup.get((scene, "tile", 32, 1))
        parallel = lookup.get((scene, "parallel", 32, 4))
        if len(references) != 1 or tile is None or parallel is None:
            skipped.append(scene)
            continue
        reference_ms = references[0]["median_ms"]
        tile_ms, parallel_ms = tile["median_ms"], parallel["median_ms"]
        fixed[scene] = dict(reference_ms=reference_ms, tile_ms=tile_ms,
                            parallel_ms=parallel_ms,
                            parallel_speedup=tile_ms / parallel_ms,
                            overall_speedup=reference_ms / parallel_ms)
    return records, fixed, skipped


def atomic_write(path, text):
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", newline="",
                                         dir=path.parent, prefix=f".{path.name}.",
                                         suffix=".tmp", delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(text)
        os.replace(temporary, path)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def main():
    default_dir = Path(__file__).resolve().parents[1] / "benchmarks" / "results"
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input-dir", type=Path, default=default_dir,
                        help="directory containing measured run_*.csv files")
    parser.add_argument("--output-dir", type=Path, default=default_dir,
                        help="directory for summary.csv and highlights.json")
    args = parser.parse_args()
    try:
        files, groups = read_runs(args.input_dir)
        records, fixed, skipped = summarize(groups)
        output = io.StringIO(newline="")
        writer = csv.DictWriter(output, fieldnames=SUMMARY_COLUMNS)
        writer.writeheader()
        writer.writerows(records)
        highlights = json.dumps(fixed, indent=2, allow_nan=False) + "\n"
        args.output_dir.mkdir(parents=True, exist_ok=True)
        atomic_write(args.output_dir / "summary.csv", output.getvalue())
        atomic_write(args.output_dir / "highlights.json", highlights)
    except (OSError, UnicodeError, csv.Error, ValueError) as error:
        parser.error(str(error))
    if skipped:
        print("Fixed Tile=32/four-worker comparison omitted for scenes with missing "
              "or ambiguous configurations: " + ", ".join(skipped), file=sys.stderr)
    print(json.dumps(dict(runs=len(files), samples=sum(len(rows) for rows in groups.values()),
                          configurations=len(records), fixed=fixed), indent=2))


if __name__ == "__main__":
    main()
