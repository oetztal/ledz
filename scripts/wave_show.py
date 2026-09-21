#!/usr/bin/env python3
"""
Render LED show previews as PNG images by driving the host-side C++ simulator.

The simulator binary at ``.pio/build/native_show_sim/program`` is built by
PlatformIO and runs the same ``src/show/*.cpp`` files that run on the device,
so every preview is byte-for-byte identical to what the strip would receive.
There is intentionally no parallel Python implementation of any show: this
script is a thin renderer that pipes the simulator's raw RGB stream into the
PNG writer and emits a gallery of previews for the GitHub Pages site.

For the full gallery and regeneration instructions see ``docs/SHOW_PREVIEWS.md``.
"""

from __future__ import annotations

import argparse
import html
import json
import shutil
import struct
import subprocess
import sys
import zlib
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable, Sequence

TAG = "wave_show"
WIDTH = 300
DEFAULT_SHOW = "Wave"

DEFAULT_SIMULATOR_BINARY = Path(".pio/build/native_show_sim/program")

# Per-iteration time advance in the legacy Python port matched Wave.cpp's
# ``time += 0.05f``. The simulator binary uses the same source file, so no
# tuning is needed here.
TIME_STEP = 0.05


# ---------------------------------------------------------------------------
# Data
# ---------------------------------------------------------------------------


@dataclass
class RenderParams:
    # Output grid
    iterations: int = 1000
    width_in: int = WIDTH

    # Show selection
    show: str = DEFAULT_SHOW
    params_json: str = "{}"

    # Wave-specific convenience flags. Applied on top of params_json so
    # existing invocations keep working without --params.
    decay_rate: float = 2.0
    brightness_frequency: float = 0.1
    mode: str = "bounce"

    # Deterministic renders for random shows
    seed: int | None = None

    # Output paths and behaviour
    output: str = "wave_show.png"
    all_dir: str | None = None
    simulator_binary: Path = DEFAULT_SIMULATOR_BINARY
    verbose: bool = False

    extra_params: dict = field(default_factory=dict)


# ---------------------------------------------------------------------------
# Validation
# ---------------------------------------------------------------------------


def _validate(params: RenderParams) -> None:
    if params.iterations < 1:
        raise SystemExit(f"{TAG}: --iterations must be >= 1")
    if params.width_in < 1:
        raise SystemExit(f"{TAG}: --width must be >= 1")
    if params.all_dir is None and not params.output:
        raise SystemExit(f"{TAG}: --output is required when not using --all")


# ---------------------------------------------------------------------------
# Simulator invocation
# ---------------------------------------------------------------------------


def ensure_simulator_built(params: RenderParams) -> Path:
    """Build the simulator binary if it is not already on disk.

    The first invocation of ``pio run -e native_show_sim`` pays the full
    compile cost (~10s on a warm cache, longer on a cold one). Subsequent
    runs hit PlatformIO's incremental cache and finish in well under a second.
    """
    binary = params.simulator_binary
    if binary.exists():
        return binary

    if not shutil.which("pio"):
        raise SystemExit(
            f"{TAG}: 'pio' was not found on $PATH. Install PlatformIO "
            "(https://platformio.org/install) and retry."
        )

    if params.verbose:
        print(f"{TAG}: building simulator via pio run -e native_show_sim",
              file=sys.stderr)

    try:
        result = subprocess.run(
            ["pio", "run", "-e", "native_show_sim"],
            check=False,
        )
    except FileNotFoundError as e:
        raise SystemExit(f"{TAG}: failed to launch pio: {e}")

    if result.returncode != 0:
        raise SystemExit(
            f"{TAG}: pio run -e native_show_sim failed (exit {result.returncode}); "
            "the simulator binary was not produced."
        )

    if not binary.exists():
        raise SystemExit(
            f"{TAG}: pio reported success but {binary} is still missing"
        )
    return binary


def invoke_simulator(params: RenderParams) -> bytes:
    """Run the simulator binary and return its raw RGB stream.

    The wire format is exactly ``params.width_in * params.iterations * 3``
    bytes — one row per iteration, three bytes per LED (R, G, B left-to-right).
    No header, no per-row framing. A short read is a simulator bug; a long
    read is a renderer bug, but we tolerate extra trailing bytes.
    """
    binary = ensure_simulator_built(params)
    argv = [
        str(binary),
        "--show", params.show,
        "--width", str(params.width_in),
        "--iterations", str(params.iterations),
        "--params", params.params_json,
    ]
    if params.seed is not None:
        argv += ["--seed", str(params.seed)]

    if params.verbose:
        print(f"{TAG}: invoking simulator: {' '.join(argv)}", file=sys.stderr)

    try:
        completed = subprocess.run(argv, check=True, capture_output=True)
    except subprocess.CalledProcessError as e:
        stderr = e.stderr.decode(errors="replace") if e.stderr else ""
        raise SystemExit(
            f"{TAG}: simulator exited {e.returncode}: {stderr.strip()}"
        )

    rgb = completed.stdout
    expected = params.width_in * params.iterations * 3
    if len(rgb) < expected:
        raise SystemExit(
            f"{TAG}: simulator emitted {len(rgb)} bytes, expected {expected}"
        )
    if len(rgb) > expected:
        rgb = rgb[:expected]
    return rgb


# ---------------------------------------------------------------------------
# PNG writer (8-bit RGB, filter type 0). Stdlib only.
# ---------------------------------------------------------------------------


def _png_chunk(tag: bytes, data: bytes) -> bytes:
    return (
        struct.pack(">I", len(data))
        + tag
        + data
        + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    )


def write_png(path: Path, width: int, height: int, rgb: bytes) -> None:
    sig = b"\x89PNG\r\n\x1a\n"
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    raw = bytearray()
    stride = width * 3
    for y in range(height):
        raw.append(0)
        raw.extend(rgb[y * stride:(y + 1) * stride])
    idat = zlib.compress(bytes(raw), 6)
    path.write_bytes(
        sig
        + _png_chunk(b"IHDR", ihdr)
        + _png_chunk(b"IDAT", idat)
        + _png_chunk(b"IEND", b"")
    )


# ---------------------------------------------------------------------------
# Orchestration
# ---------------------------------------------------------------------------


def render_one(params: RenderParams) -> Path:
    """Build the simulator if needed, invoke it, write the PNG.

    Returns the path of the PNG that was written.
    """
    rgb = invoke_simulator(params)
    out_path = Path(params.output)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    write_png(out_path, params.width_in, params.iterations, rgb)
    print(f"{TAG}: wrote {out_path} ({params.width_in}x{params.iterations})")
    return out_path


def list_registered_shows(binary: Path) -> list[str]:
    completed = subprocess.run(
        [str(binary), "--list"],
        check=True, capture_output=True,
    )
    return [line.strip() for line in completed.stdout.decode().splitlines() if line.strip()]


def render_all(params: RenderParams) -> list[Path]:
    """Render one PNG per registered show into ``params.all_dir``.

    Each show is rendered in a separate simulator process so per-process state
    (--seed, construction-time RNG) starts clean. An ``index.html`` enumerates
    every preview with a hyperlink.
    """
    out_dir = Path(params.all_dir or "show_previews")
    out_dir.mkdir(parents=True, exist_ok=True)

    binary = ensure_simulator_built(params)
    names = list_registered_shows(binary)

    written: list[Path] = []
    for name in names:
        per_show = RenderParams(
            iterations=params.iterations,
            width_in=params.width_in,
            show=name,
            params_json="{}",
            seed=params.seed,
            simulator_binary=binary,
            output=str(out_dir / f"{name}.png"),
            verbose=params.verbose,
        )
        if params.verbose:
            print(f"{TAG}: rendering {name}", file=sys.stderr)
        render_one(per_show)
        written.append(out_dir / f"{name}.png")

    _write_index_html(out_dir, written, params)
    return written


def _write_index_html(out_dir: Path, pngs: Sequence[Path], params: RenderParams) -> None:
    rows = []
    for png in pngs:
        show_name = png.stem
        rows.append(
            f'<li><a href="{html.escape(png.name)}">{html.escape(show_name)}</a></li>'
        )
    seed_text = f"seed={params.seed}" if params.seed is not None else "no seed"
    doc = (
        "<!doctype html>\n"
        '<html lang="en">\n'
        "<head>\n"
        '  <meta charset="utf-8">\n'
        "  <title>ledz show previews</title>\n"
        "  <style>\n"
        "    body { font-family: system-ui, sans-serif; max-width: 60rem; margin: 2rem auto; padding: 0 1rem; }\n"
        "    h1 { margin-bottom: 0.2rem; }\n"
        "    .meta { color: #666; margin-bottom: 1.5rem; }\n"
        "    ul { list-style: none; padding: 0; display: grid; gap: 0.5rem; }\n"
        "    li a { display: inline-block; padding: 0.4rem 0.6rem; background: #f4f4f4; border-radius: 4px; text-decoration: none; color: #0645ad; }\n"
        "    li a:hover { background: #e8e8e8; }\n"
        "  </style>\n"
        "</head>\n"
        "<body>\n"
        "  <h1>ledz show previews</h1>\n"
        f'  <p class="meta">Generated from <code>src/show/*.cpp</code> via <code>native_show_sim</code> &middot; {seed_text} &middot; width={params.width_in} &middot; iterations={params.iterations}</p>\n'
        f"  <ul>\n    {' '.join(rows)}\n  </ul>\n"
        "</body>\n"
        "</html>\n"
    )
    (out_dir / "index.html").write_text(doc)


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="wave_show",
        description="Render ledz show previews via the host-side C++ simulator.",
    )
    p.add_argument("-n", "--iterations", type=int, default=1000,
                   help="number of rows (iterations); default 1000")
    p.add_argument("-W", "--width", type=int, default=WIDTH,
                   help=f"image width in pixels; default {WIDTH}")
    p.add_argument("-o", "--output", default="wave_show.png",
                   help="output PNG path; default wave_show.png")

    p.add_argument("--show", default=DEFAULT_SHOW,
                   help=f"show to render; default {DEFAULT_SHOW}")
    p.add_argument("--params", default=None,
                   help='JSON parameters for the show; default "{}"')
    p.add_argument("--seed", type=int, default=None,
                   help="RNG seed for deterministic random shows (Fire, Starlight, ...)")

    p.add_argument("--wave", dest="wave", default=None,
                   help="deprecated: legacy alias for --show wave")
    g = p.add_argument_group("wave parameters (only used when --show=Wave)")
    g.add_argument("--decay-rate", dest="decay_rate", type=float, default=2.0,
                   help="exponential decay with distance from source; default 2.0")
    g.add_argument("--brightness-frequency", dest="brightness_frequency",
                   type=float, default=0.1,
                   help="Hz of source bounce and hue cycling; default 0.1")
    g.add_argument("--mode", default="bounce",
                   help="phase mode (no-op; kept for backward compatibility)")

    p.add_argument("--simulator-binary", dest="simulator_binary",
                   default=str(DEFAULT_SIMULATOR_BINARY),
                   help="path to the simulator binary; default "
                        f"{DEFAULT_SIMULATOR_BINARY}")
    p.add_argument("--all", dest="all_dir", default=None, metavar="DIR",
                   help="render one PNG per registered show into DIR and "
                        "emit DIR/index.html")
    p.add_argument("--list", action="store_true",
                   help="list every show currently registered in the "
                        "ShowFactory and exit")
    p.add_argument("-v", "--verbose", action="store_true")
    return p


def _resolve_params_json(args: argparse.Namespace) -> str:
    if args.params is not None:
        return args.params
    # Build the Wave param JSON from the legacy CLI flags. Other shows ignore
    # the Wave-specific fields, so passing them is harmless; the renderer
    # still respects --params when supplied.
    payload = {
        "decay_rate": args.decay_rate,
        "brightness_frequency": args.brightness_frequency,
        "mode": args.mode,
    }
    return json.dumps(payload)


def _resolve_show(args: argparse.Namespace) -> str:
    if args.wave is not None:
        if args.wave != "wave":
            raise SystemExit(
                f"{TAG}: --wave {args.wave!r} is no longer supported; "
                "use --show <name> instead"
            )
        return "Wave"
    return args.show


def params_from_args(args: argparse.Namespace) -> RenderParams:
    return RenderParams(
        iterations=args.iterations,
        width_in=args.width,
        show=_resolve_show(args),
        params_json=_resolve_params_json(args),
        decay_rate=args.decay_rate,
        brightness_frequency=args.brightness_frequency,
        mode=args.mode,
        seed=args.seed,
        output=args.output,
        all_dir=args.all_dir,
        simulator_binary=Path(args.simulator_binary),
        verbose=args.verbose,
    )


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    if args.list:
        binary = ensure_simulator_built(RenderParams(verbose=args.verbose))
        for name in list_registered_shows(binary):
            print(name)
        return 0

    params = params_from_args(args)
    _validate(params)

    if params.all_dir is not None:
        render_all(params)
        return 0

    render_one(params)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
