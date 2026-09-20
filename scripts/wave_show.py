#!/usr/bin/env python3
"""
Simulate the ledz "Wave" show as a 2D pixel image.

The reference implementation lives at https://github.com/oetztal/ledz
(src/show/Wave.cpp). This script is a faithful, stdlib-only Python port:
the per-iteration pixel values match what the C++ version would write to
the strip when sampled at the same iteration index.

The output picture is WIDTH pixels wide (default 300, matching a typical
LED strip); each row is one iteration of the show, so the y-axis is time.

Per-iteration model (one row y):
  - time and color_time advance by TIME_STEP (0.05) per iteration
  - a single source position bounces between the two ends of the strip:
        source_pos = (W-1)/2 * (1 - cos(time * brightness_frequency * 2π))
  - brightness decays exponentially with distance from the source
  - the source brightness oscillates subtly (0.65 + 0.35 * sin(...))
  - hue comes from the NeoPixel rainbow wheel indexed by the time at
    which the wavefront currently at pixel i was emitted
  - the final pixel = wheel(emission_index) * source_brightness * envelope

The ledz reference additionally modulates brightness by |sin(phase)|
where phase is `distance * 2π / wavelength`, producing fine stripes.
The wavelength parameter is intentionally omitted here: with a typical
strip width the resulting structure is too short to be useful in the
preview, so the picture shows only the bouncing source, its decay
envelope, and the hue trail.

The script exists to experiment with parameter sets and to preview new
show types before any firmware work. It is stdlib-only: it writes PNG
itself (no numpy / Pillow required). For larger grids or many presets,
swap the inner loop of ``render`` for numpy.

Adding new shows: write a ``(x, y, params) -> (r, g, b)`` function and
register it in WAVES; expose its parameters in build_parser().
"""

from __future__ import annotations

import argparse
import math
import struct
import sys
import zlib
from dataclasses import dataclass
from pathlib import Path

TAG = "wave_show"
WIDTH = 300

# Per-iteration time advance; matches ledz Wave.cpp's `time += 0.05f`.
# Kept as a module constant so the default picture is byte-for-byte
# reproducible against the reference.
TIME_STEP = 0.05

# Wave mode constants. Names match ledz's WaveMode enum.
MODE_BOUNCE = "bounce"
MODE_TRAVELING = "traveling"
MODES = (MODE_BOUNCE, MODE_TRAVELING)


# ---------------------------------------------------------------------------
# Colour
# ---------------------------------------------------------------------------


def wheel(wheel_pos: int) -> tuple[int, int, int]:
    """HSV rainbow wheel at full saturation/brightness.

    wheel_pos in [0, 254] -> (r, g, b).
    Walks the faces of the RGB cube, so complementary colours (yellow at
    pos~42, cyan at ~127, magenta at ~212) get the same weight as the
    primaries. Replaces the Adafruit edge-walking wheel from ledz, which
    cannot produce pure yellow/cyan/magenta.
    """
    if wheel_pos > 254:
        wheel_pos = 254
    if wheel_pos < 0:
        wheel_pos = 0
    h = wheel_pos * 360.0 / 255.0
    c = 1.0                                # chroma = v*s with v=s=1
    x = c * (1.0 - abs((h / 60.0) % 2.0 - 1.0))
    m = 0.0
    if   h <  60: rp, gp, bp = c, x, 0.0
    elif h < 120: rp, gp, bp = x, c, 0.0
    elif h < 180: rp, gp, bp = 0.0, c, x
    elif h < 240: rp, gp, bp = 0.0, x, c
    elif h < 300: rp, gp, bp = x, 0.0, c
    else:        rp, gp, bp = c, 0.0, x
    return (
        min(255, int(rp * 255 + 0.5)),
        min(255, int(gp * 255 + 0.5)),
        min(255, int(bp * 255 + 0.5)),
    )


# ---------------------------------------------------------------------------
# Wave show
# ---------------------------------------------------------------------------


def wave_show(x: int, y: int, p: "WaveParams") -> tuple[int, int, int]:
    """Compute the (r, g, b) for pixel (x, y) under the ledz Wave show.

    y is the iteration index (0-based); x is the LED index in [0, WIDTH-1].
    The wavelength-based |sin(phase)| modulation from the ledz reference
    is intentionally omitted (see module docstring).
    """
    w = p.width_in
    time = (y + 1) * TIME_STEP
    color_time = time

    omega = p.brightness_frequency * 2.0 * math.pi
    # cosine bounce: oscillates between 0 and W-1 with continuous velocity
    source_pos = (w - 1) / 2.0 * (1.0 - math.cos(time * omega))

    # subtle source brightness oscillation: in [0.30, 1.00]
    source_brightness = 0.65 + 0.35 * math.sin(time * omega)

    inv_num_leds = 1.0 / float(w)

    distance = x - source_pos
    abs_distance = abs(distance)
    envelope = math.exp(-p.decay_rate * abs_distance * inv_num_leds)

    # Hue from emission time: wavefront at pixel x was emitted
    # ~|x - source_pos| / (W * brightness_frequency) seconds ago.
    propagation_speed = float(w) * p.brightness_frequency
    emission_time = color_time - abs_distance / propagation_speed
    # ledz uses `int(emission_time * 20) % 255`; Python's % on negatives
    # differs from C's, so normalise into [0, 254] explicitly.
    color_index = int(emission_time * 20.0) % 255
    if color_index < 0:
        color_index += 255
    r0, g0, b0 = wheel(color_index)

    final = source_brightness * envelope
    return (
        min(255, int(r0 * final)),
        min(255, int(g0 * final)),
        min(255, int(b0 * final)),
    )


WAVES: dict[str, callable] = {
    "wave": wave_show,
}


# ---------------------------------------------------------------------------
# Rendering
# ---------------------------------------------------------------------------


@dataclass
class WaveParams:
    # picture
    iterations: int = 1000
    width_in: int = WIDTH

    # show selection (registry key; only "wave" exists today)
    wave: str = "wave"

    # ledz Wave constructor parameters (wavelength intentionally omitted;
    # see wave_show() and the module docstring)
    decay_rate: float = 2.0
    brightness_frequency: float = 0.1
    mode: str = MODE_BOUNCE

    output: str = "wave_show.png"
    verbose: bool = False


def _validate(params: WaveParams) -> None:
    if params.wave not in WAVES:
        raise SystemExit(
            f"{TAG}: unknown wave {params.wave!r}; choose from {sorted(WAVES)}"
        )
    if params.mode not in MODES:
        raise SystemExit(
            f"{TAG}: unknown mode {params.mode!r}; choose from {MODES}"
        )
    if params.iterations < 1:
        raise SystemExit(f"{TAG}: --iterations must be >= 1")
    if params.width_in < 1:
        raise SystemExit(f"{TAG}: --width must be >= 1")


def render(params: WaveParams) -> bytes:
    """Render the wave to ``width * height * 3`` RGB bytes (row-major)."""
    fn = WAVES[params.wave]
    w, h = params.width_in, params.iterations
    out = bytearray(w * h * 3)
    for y in range(h):
        row = y * w * 3
        for x in range(w):
            r, g, b = fn(x, y, params)
            o = row + x * 3
            out[o] = r
            out[o + 1] = g
            out[o + 2] = b
        if params.verbose and y % max(1, h // 10) == 0:
            print(f"{TAG}: row {y}/{h}", file=sys.stderr)
    return bytes(out)


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
# CLI
# ---------------------------------------------------------------------------


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="wave_show",
        description=__doc__.splitlines()[0] if __doc__ else "wave show simulator",
    )
    p.add_argument("-n", "--iterations", type=int, default=1000,
                   help="number of rows (iterations); default 1000 "
                        "(five full bounces at the default frequency)")
    p.add_argument("-W", "--width", type=int, default=WIDTH,
                   help=f"image width in pixels; default {WIDTH}")
    p.add_argument("-o", "--output", default="wave_show.png",
                   help="output PNG path; default wave_show.png")
    p.add_argument("--wave", choices=sorted(WAVES), default="wave",
                   help="show type; default wave")

    g = p.add_argument_group("wave parameters (ledz Wave constructor defaults)")
    g.add_argument("--decay-rate", dest="decay_rate", type=float, default=2.0,
                   help="exponential decay with distance from source; default 2.0")
    g.add_argument("--brightness-frequency", dest="brightness_frequency",
                   type=float, default=0.1,
                   help="Hz of source bounce and hue cycling; default 0.1 "
                        "(200-iteration period)")
    g.add_argument("--mode", choices=MODES, default=MODE_BOUNCE,
                   help="phase mode (kept for future expansion; "
                        "currently both modes render identically since "
                        "the wavelength modulation is omitted)")

    p.add_argument("--list", action="store_true",
                   help="list available wave types and modes and exit")
    p.add_argument("-v", "--verbose", action="store_true")
    return p


def params_from_args(args: argparse.Namespace) -> WaveParams:
    return WaveParams(
        iterations=args.iterations,
        width_in=args.width,
        decay_rate=args.decay_rate,
        brightness_frequency=args.brightness_frequency,
        mode=args.mode,
        output=args.output,
        verbose=args.verbose,
        wave=args.wave,
    )


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    if args.list:
        print("waves:")
        for name in WAVES:
            print(f"  {name}")
        print("modes:")
        for name in MODES:
            print(f"  {name}")
        return 0

    params = params_from_args(args)
    _validate(params)

    if params.verbose:
        print(f"{TAG}: rendering {params.width_in}x{params.iterations} "
              f"mode={params.mode}", file=sys.stderr)

    rgb = render(params)
    out_path = Path(params.output)
    write_png(out_path, params.width_in, params.iterations, rgb)
    print(f"{TAG}: wrote {out_path} ({params.width_in}x{params.iterations})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
