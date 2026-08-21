#!/usr/bin/env python3
"""Build an lcov coverage report from the native test run.

Run the tests first, then this:

    pio test -e native
    python3 scripts/coverage_report.py        # or: pio run -e native -t coverage

This is the only implementation. `coverage_link.py` registers the `coverage`
custom target, which shells out to this file; CI calls it directly. There is
deliberately no "generate automatically after tests" hook: PlatformIO builds
and runs each test directory in turn, so no SCons action can fire after the
*last* test binary has run, which is the only point where the .gcda files are
complete. The previous post-action on a target named "test" never fired at all.
"""

import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD_DIR = os.path.join(ROOT, ".pio", "build", "native")
INFO = os.path.join(ROOT, "coverage.info")
INFO_FILTERED = os.path.join(ROOT, "coverage_filtered.info")
HTML_DIR = os.path.join(ROOT, "coverage_report")

# Apple's clang and gcc both emit data that lcov 2.x flags over. None of these
# categories indicate a broken report: they are inlined/implicit destructors in
# headers and __cxx_global_var_init entries with no line number. Refusing to
# emit a report because of them is what left this project with no report at all.
IGNORE_V2 = "gcov,source,mismatch,unsupported,negative,inconsistent,format,empty,unused"
IGNORE_V1 = "gcov,source,graph"


def run(cmd, **kwargs):
    print("+ " + " ".join(cmd))
    return subprocess.run(cmd, **kwargs)


def lcov_major():
    try:
        out = subprocess.run(["lcov", "--version"], capture_output=True, text=True).stdout
    except FileNotFoundError:
        return None
    for token in out.replace("-", " ").split():
        if token[:1].isdigit():
            try:
                return int(token.split(".")[0])
            except ValueError:
                continue
    return 1


def main():
    if shutil.which("lcov") is None or shutil.which("genhtml") is None:
        print("error: lcov/genhtml not found. Install it (macOS: brew install lcov, "
              "Debian/Ubuntu: apt-get install lcov).", file=sys.stderr)
        return 1

    if not os.path.isdir(BUILD_DIR):
        print(f"error: {BUILD_DIR} does not exist. Run 'pio test -e native' first.", file=sys.stderr)
        return 1

    gcda = [os.path.join(d, f)
            for d, _, files in os.walk(BUILD_DIR)
            for f in files if f.endswith(".gcda")]
    if not gcda:
        print("error: no .gcda files under .pio/build/native. Run 'pio test -e native' "
              "first, and check that --coverage is still in the native build_flags.",
              file=sys.stderr)
        return 1
    print(f"Found {len(gcda)} .gcda files")

    ignore = IGNORE_V2 if (lcov_major() or 1) >= 2 else IGNORE_V1
    # gcov must match the compiler that produced the data. On macOS the gcov on
    # PATH is Apple's llvm-cov shim, on Linux it is gcc's; both are correct for
    # their own toolchain. GCOV overrides for mixed setups.
    gcov = os.environ.get("GCOV", "gcov")

    capture = run([
        "lcov", "--capture",
        "--directory", BUILD_DIR,
        "--base-directory", ROOT,
        "--gcov-tool", gcov,
        "--output-file", INFO,
        "--ignore-errors", ignore,
        "--quiet",
    ])
    if capture.returncode != 0 or not os.path.exists(INFO):
        print("error: lcov capture failed", file=sys.stderr)
        return 1

    # Keep only this project's own sources. --extract is an allowlist, so new
    # dependency or toolchain paths cannot silently creep into the report the
    # way they do with a --remove denylist.
    if run(["lcov", "--extract", INFO, os.path.join(ROOT, "src", "*"),
            "--output-file", INFO_FILTERED,
            "--ignore-errors", ignore, "--quiet"]).returncode != 0:
        print("error: lcov extract failed", file=sys.stderr)
        return 1

    # Generated web-asset headers are machine-written and excluded from Sonar
    # too (sonar-project.properties), so keep the two consistent.
    if run(["lcov", "--remove", INFO_FILTERED, os.path.join(ROOT, "src", "generated", "*"),
            "--output-file", INFO_FILTERED,
            "--ignore-errors", ignore, "--quiet"]).returncode != 0:
        print("error: lcov remove failed", file=sys.stderr)
        return 1

    if run(["genhtml", INFO_FILTERED,
            "--output-directory", HTML_DIR,
            "--ignore-errors", "source,inconsistent,format,category,unsupported,empty",
            "--quiet"]).returncode != 0:
        print("error: genhtml failed", file=sys.stderr)
        return 1

    summary = run(["lcov", "--summary", INFO_FILTERED,
                   "--ignore-errors", ignore + ",corrupt"],
                  capture_output=True, text=True)
    print(summary.stdout.strip() or summary.stderr.strip())

    print(f"\nCoverage report: {os.path.join(HTML_DIR, 'index.html')}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
