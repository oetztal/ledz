#!/usr/bin/env python3
"""
Build the ledz GitHub Pages site.

The script does three things:

1. Render LED show previews as PNGs by driving the host-side C++ simulator.
2. Stitch those PNGs into ``docs/show_previews/index.html``, one section per
   show with all its variants inlined.
3. Render the site landing page (``docs/index.html``) from ``README.md`` and
   turn every other ``docs/*.md`` file into a browsable HTML page.

The simulator binary at ``.pio/build/native_show_sim/program`` is built by
PlatformIO and runs the same ``src/show/*.cpp`` files that run on the device,
so every preview is byte-for-byte identical to what the strip would receive.
There is intentionally no parallel Python implementation of any show: this
script is a thin renderer that pipes the simulator's raw RGB stream into the
PNG writer.

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

TAG = "build_pages"
WIDTH = 300
DEFAULT_SHOW = "Wave"

DEFAULT_SIMULATOR_BINARY = Path(".pio/build/native_show_sim/program")

# Per-iteration time advance in the legacy Python port matched Wave.cpp's
# ``time += 0.05f``. The simulator binary uses the same source file, so no
# tuning is needed here.
TIME_STEP = 0.05

DEFAULT_VARIANTS_FILE = Path("scripts/show_variants.json")


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
    output: str = "show_preview.png"
    all_dir: str | None = None
    simulator_binary: Path = DEFAULT_SIMULATOR_BINARY
    verbose: bool = False
    variants_file: Path = DEFAULT_VARIANTS_FILE

    extra_params: dict = field(default_factory=dict)


# ---------------------------------------------------------------------------
# Variants
# ---------------------------------------------------------------------------


@dataclass
class Variant:
    name: str
    label: str
    params: dict
    iterations: int | None = None

    def png_filename(self, show: str) -> str:
        # ``{show}_{variant}.png``. Avoid duplicating the show name when the
        # variant already encodes it (e.g. Wave's "default" -> "Wave_default.png"
        # is fine, but we keep the form stable for the gallery links).
        return f"{show}_{self.name}.png"


def _strip_comment_key(d: dict) -> dict:
    return {k: v for k, v in d.items() if not k.startswith("_")}


def load_variants(path: Path) -> dict[str, tuple[Variant | None, list[Variant]]]:
    """Return ``{show_name: (default_variant_or_None, [Variant, ...])}``.

    The file format groups everything under each show name:

    .. code-block:: json

        {
            "Wave": {
                "description": "...",
                "default": {"params": {...}, "iterations": 100},
                "variants": [
                    {"name": "tight", "label": "Tight", "params": {...}},
                    ...
                ]
            }
        }

    The ``default`` object holds the factory default parameters, structurally
    distinct from the curated ``variants``. When present, ``load_variants``
    synthesises a ``Variant(name="default", label="Factory default", ...)``
    from ``body["default"]["params"]`` (and its optional ``iterations``
    override) and returns it as the first element of the tuple; the second
    element is the list of curated ``variants`` entries. ``_resolve_variants``
    is responsible for prepending the default to the rendered list.

    A show that is missing from the file falls back to a single ``default``
    variant with empty parameters so the gallery still shows every registered
    show. This means the file only has to mention shows that have multiple
    interesting parameter sets.
    """
    if not path.exists():
        return {}
    with path.open() as f:
        raw = json.load(f)
    raw = _strip_comment_key(raw)

    out: dict[str, tuple[Variant | None, list[Variant]]] = {}
    for show, body in raw.items():
        if not isinstance(body, dict) or "variants" not in body:
            # Tolerate bare-list shorthand: {"Wave": [{...}, {...}]}.
            if isinstance(body, list):
                variants = [
                    Variant(**{k: v for k, v in v.items() if k != "description"})
                    for v in body
                ]
                out[show] = (None, variants)
            continue
        body_iterations = body.get("iterations")

        default_variant: Variant | None = None
        raw_default = body.get("default")
        if isinstance(raw_default, dict):
            default_variant = Variant(
                name="default",
                label="Factory default",
                params=dict(raw_default.get("params", {})),
                iterations=raw_default.get("iterations", body_iterations),
            )

        variants: list[Variant] = []
        for v in body["variants"]:
            resolved = v["iterations"] if "iterations" in v else body_iterations
            variants.append(Variant(
                name=str(v["name"]),
                label=str(v.get("label", v["name"])),
                params=dict(v.get("params", {})),
                iterations=resolved,
            ))
        out[show] = (default_variant, variants)
    return out


def fallback_variant(show: str) -> Variant:
    """A single default variant for shows not in the variants file."""
    return Variant(name="default", label="Default", params={})


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


@dataclass
class VariantEntry:
    show: str
    variant: Variant
    png: Path
    description: str


def _resolve_variants(
    show: str,
    variants_by_show: dict[str, tuple[Variant | None, list[Variant]]],
    descriptions: dict[str, str],
) -> tuple[list[Variant], str]:
    """Return ``(variants, description)`` for a show, falling back to a single
    default variant when ``show`` is not mentioned in the variants file.

    The synthetic ``default`` variant (when present) is prepended to the list
    of curated ``variants`` so the default appears first under each show's
    section in the gallery. Callers see the default as the first element of
    the returned list and don't need to know which entry is the default.
    """
    if show in variants_by_show:
        default_variant, variants = variants_by_show[show]
        out = list(variants)
        if default_variant is not None:
            out.insert(0, default_variant)
        return out, descriptions.get(show, "")
    return [fallback_variant(show)], descriptions.get(show, "")


def render_all(params: RenderParams) -> list[VariantEntry]:
    """Render one PNG per variant per registered show into ``params.all_dir``.

    Each show is rendered in a separate simulator process so per-process state
    (--seed, construction-time RNG) starts clean. Every variant gets its own
    ``{show}_{variant}.png``. The gallery ``index.html`` is written at the end
    and embeds every preview as an inline ``<img>`` so the page is a
    self-contained snapshot of the build.
    """
    out_dir = Path(params.all_dir or "show_previews")
    out_dir.mkdir(parents=True, exist_ok=True)

    binary = ensure_simulator_built(params)
    names = list_registered_shows(binary)
    variants_by_show = load_variants(params.variants_file)
    descriptions = _load_descriptions(params.variants_file)

    written: list[VariantEntry] = []
    for name in names:
        variants, description = _resolve_variants(name, variants_by_show, descriptions)
        for variant in variants:
            per_variant = RenderParams(
                iterations=variant.iterations if variant.iterations is not None else params.iterations,
                width_in=params.width_in,
                show=name,
                params_json=json.dumps(variant.params),
                seed=params.seed,
                simulator_binary=binary,
                output=str(out_dir / variant.png_filename(name)),
                verbose=params.verbose,
            )
            if params.verbose:
                print(f"{TAG}: rendering {name} ({variant.name})", file=sys.stderr)
            render_one(per_variant)
            written.append(VariantEntry(
                show=name,
                variant=variant,
                png=out_dir / variant.png_filename(name),
                description=description,
            ))

    _write_gallery_index(out_dir, written, params)
    return written


def _load_descriptions(path: Path) -> dict[str, str]:
    """Return ``{show: description}`` from the variants file. Shows without a
    description map to an empty string."""
    if not path.exists():
        return {}
    with path.open() as f:
        raw = json.load(f)
    raw = _strip_comment_key(raw)
    out: dict[str, str] = {}
    for show, body in raw.items():
        if isinstance(body, dict) and "description" in body:
            out[show] = str(body["description"])
    return out


# ---------------------------------------------------------------------------
# HTML rendering for the gallery and the site landing page
# ---------------------------------------------------------------------------


GALLERY_CSS = """
:root { color-scheme: light dark; }
body {
  font-family: system-ui, -apple-system, "Segoe UI", sans-serif;
  max-width: 72rem; margin: 0 auto; padding: 1.5rem 1.25rem 4rem;
  line-height: 1.45;
}
h1 { margin-bottom: 0.2rem; }
h1 + p { color: #555; margin-top: 0; }
.meta { color: #777; font-size: 0.9rem; margin-bottom: 2rem; }
.meta code { font-size: 0.85em; }
nav.toc { columns: 2; -webkit-columns: 2; -moz-columns: 2; column-gap: 1.5rem;
          margin: 0 0 2.5rem; padding: 0; list-style: none; }
nav.toc li { break-inside: avoid; }
nav.toc a { text-decoration: none; color: #0645ad; }
nav.toc a:hover { text-decoration: underline; }
section.show {
  border-top: 1px solid #e1e1e1;
  padding-top: 1.5rem;
  margin-top: 2rem;
}
section.show > h2 { margin: 0 0 0.25rem; }
section.show > p.description { color: #555; margin: 0 0 1rem; font-style: italic; }
.variants { display: grid; gap: 1.25rem;
            grid-template-columns: repeat(auto-fill, minmax(18rem, 1fr)); }
.variant {
  border: 1px solid #e1e1e1; border-radius: 6px; padding: 0.75rem;
  background: #fafafa;
}
.variant h3 { margin: 0 0 0.5rem; font-size: 1rem; }
.variant .preview {
  display: block; width: 100%; height: auto; image-rendering: pixelated;
  background: #000; border-radius: 3px;
}
.variant details { margin-top: 0.5rem; font-size: 0.85rem; }
.variant summary { cursor: pointer; color: #555; }
.variant pre {
  margin: 0.5rem 0 0; padding: 0.5rem; background: #fff;
  border: 1px solid #e1e1e1; border-radius: 3px;
  font-size: 0.8rem; overflow-x: auto;
}
@media (prefers-color-scheme: dark) {
  body { background: #161616; color: #e6e6e6; }
  h1 + p, .meta, section.show > p.description, .variant summary { color: #aaa; }
  section.show { border-top-color: #2a2a2a; }
  .variant { background: #1f1f1f; border-color: #2a2a2a; }
  .variant pre { background: #161616; border-color: #2a2a2a; }
  nav.toc a { color: #7eb3ff; }
}
"""


def _format_params(params: dict) -> str:
    if not params:
        return "factory defaults"
    return json.dumps(params, indent=2, sort_keys=True, separators=(",", ": "))


def _variant_section_html(entry: VariantEntry) -> str:
    img_rel = entry.png.name
    label = html.escape(entry.variant.label)
    name = html.escape(entry.variant.name)
    params_text = html.escape(_format_params(entry.variant.params))
    show = html.escape(entry.show)
    return (
        f'<div class="variant" id="{show}-{name}">\n'
        f'  <h3>{label}</h3>\n'
        f'  <img class="preview" src="{html.escape(img_rel)}" '
        f'alt="{show} show preview, {label} variant" loading="lazy">\n'
        f'  <details><summary>Parameters</summary>\n'
        f'    <pre>{params_text}</pre>\n'
        f'  </details>\n'
        f'</div>'
    )


def _group_by_show(entries: Sequence[VariantEntry]) -> list[tuple[str, str, list[VariantEntry]]]:
    grouped: dict[str, list[VariantEntry]] = {}
    description_by_show: dict[str, str] = {}
    for entry in entries:
        grouped.setdefault(entry.show, []).append(entry)
        if entry.description:
            description_by_show[entry.show] = entry.description
    return [
        (show, description_by_show.get(show, ""), variants)
        for show, variants in grouped.items()
    ]


def _write_gallery_index(
    out_dir: Path, entries: Sequence[VariantEntry], params: RenderParams,
) -> None:
    """Render ``show_previews/index.html`` with every preview embedded inline.

    The HTML lists each show as a section, with all its variants stacked
    underneath in a responsive grid. Each variant block carries an ``<img>``
    back to its PNG plus the exact JSON parameters that produced it, so the
    gallery documents both the look and the recipe.
    """
    seed_text = f"seed={params.seed}" if params.seed is not None else "no seed"
    toc_rows = []
    grouped = _group_by_show(entries)
    for show, _, _ in grouped:
        anchor = html.escape(show)
        toc_rows.append(
            f'    <li><a href="#{anchor}">{html.escape(show)}</a></li>'
        )

    sections = []
    for show, description, variants in grouped:
        variant_blocks = "\n".join(_variant_section_html(v) for v in variants)
        section = (
            f'<section class="show" id="{html.escape(show)}">\n'
            f'  <h2>{html.escape(show)}</h2>\n'
        )
        if description:
            section += f'  <p class="description">{html.escape(description)}</p>\n'
        section += f'  <div class="variants">\n{variant_blocks}\n  </div>\n</section>'
        sections.append(section)

    doc = (
        "<!doctype html>\n"
        '<html lang="en">\n'
        "<head>\n"
        '  <meta charset="utf-8">\n'
        '  <meta name="viewport" content="width=device-width, initial-scale=1">\n'
        "  <title>ledz show previews</title>\n"
        f"  <style>{GALLERY_CSS}</style>\n"
        "</head>\n"
        "<body>\n"
        '  <p><a href="../index.html">&larr; ledz home</a></p>\n'
        "  <h1>ledz show previews</h1>\n"
        "  <p>Every show the firmware supports, rendered by the same C++ "
        "source that runs on the device. Each preview is a width&times;"
        "iterations grid where the x-axis is the LED index and the y-axis is "
        "the iteration count in 10 ms steps.</p>\n"
        f'  <p class="meta">Generated from <code>src/show/*.cpp</code> via '
        f'<code>native_show_sim</code> &middot; {seed_text} &middot; '
        f'width={params.width_in} &middot; iterations={params.iterations}</p>\n'
        "  <nav><ul class=\"toc\">\n"
        + "\n".join(toc_rows) + "\n"
        "  </ul></nav>\n"
        + "\n".join(sections) + "\n"
        "</body>\n"
        "</html>\n"
    )
    (out_dir / "index.html").write_text(doc)


# ---------------------------------------------------------------------------
# Landing page: docs/index.html rendered from README.md
# ---------------------------------------------------------------------------


LANDING_CSS = """
:root { color-scheme: light dark; }
body {
  font-family: system-ui, -apple-system, "Segoe UI", sans-serif;
  max-width: 52rem; margin: 0 auto; padding: 1.5rem 1.25rem 4rem;
  line-height: 1.55;
}
h1 { margin: 0 0 0.25rem; font-size: 2rem; }
h2 { margin-top: 2rem; border-bottom: 1px solid #e1e1e1; padding-bottom: 0.25rem; }
h3 { margin-top: 1.5rem; }
.tagline { color: #555; margin: 0 0 1.5rem; font-size: 1.05rem; }
.cards { display: grid; gap: 0.75rem;
         grid-template-columns: repeat(auto-fill, minmax(15rem, 1fr));
         margin: 1.25rem 0 2rem; padding: 0; list-style: none; }
.cards li a { display: block; padding: 0.85rem 1rem;
              border: 1px solid #e1e1e1; border-radius: 6px;
              background: #fafafa; color: #0645ad;
              text-decoration: none; }
.cards li a strong { display: block; margin-bottom: 0.2rem; color: inherit; }
.cards li a span { color: #555; font-size: 0.9rem; }
.cards li a:hover { border-color: #0645ad; }
pre {
  background: #f4f4f4; padding: 0.75rem 1rem; border-radius: 4px;
  overflow-x: auto; font-size: 0.85rem;
}
code { font-family: ui-monospace, "SF Mono", Menlo, Consolas, monospace; font-size: 0.9em; }
pre code { font-size: inherit; }
:not(pre) > code { background: #f0f0f0; padding: 0.1em 0.3em; border-radius: 3px; }
table { border-collapse: collapse; margin: 0.75rem 0 1.25rem; }
th, td { border: 1px solid #d0d0d0; padding: 0.4rem 0.65rem; text-align: left; }
th { background: #f4f4f4; }
blockquote { border-left: 3px solid #ccc; margin: 1rem 0; padding: 0.25rem 0 0.25rem 1rem; color: #555; }
img { max-width: 100%; height: auto; }
@media (prefers-color-scheme: dark) {
  body { background: #161616; color: #e6e6e6; }
  h2 { border-bottom-color: #2a2a2a; }
  .tagline, .cards li a span { color: #aaa; }
  .cards li a { background: #1f1f1f; border-color: #2a2a2a; color: #7eb3ff; }
  .cards li a:hover { border-color: #7eb3ff; }
  pre { background: #1f1f1f; }
  :not(pre) > code { background: #2a2a2a; }
  th { background: #1f1f1f; }
  th, td { border-color: #2a2a2a; }
}
"""


@dataclass
class LandingCard:
    title: str
    blurb: str
    href: str


LANDING_CARDS: list[LandingCard] = [
    LandingCard(
        title="Show previews gallery",
        blurb="A live PNG of every show, generated by the firmware's own C++.",
        href="show_previews/",
    ),
    LandingCard(
        title="Show parameters",
        blurb="Every JSON parameter the API accepts, with examples.",
        href="SHOW_PARAMETERS.html",
    ),
    LandingCard(
        title="OTA firmware updates",
        blurb="How over-the-air updates from GitHub releases work on the device.",
        href="OTA_FIRMWARE_UPDATES.html",
    ),
    LandingCard(
        title="OTA quick start",
        blurb="Five-minute integration guide for shipping updates.",
        href="OTA_QUICK_START.html",
    ),
    LandingCard(
        title="mDNS discovery",
        blurb="How the device advertises itself as ledz-XXXXXX.local.",
        href="MDNS.html",
    ),
    LandingCard(
        title="Releasing",
        blurb="How to cut a release and trigger the OTA pipeline.",
        href="RELEASING.html",
    ),
    LandingCard(
        title="Source on GitHub",
        blurb="Issues, PRs, and the full project history.",
        href="https://github.com/oetztal/ledz",
    ),
]


def _render_landing_cards() -> str:
    items = []
    for card in LANDING_CARDS:
        title = html.escape(card.title)
        blurb = html.escape(card.blurb)
        href = html.escape(card.href, quote=True)
        items.append(
            f'    <li><a href="{href}"><strong>{title}</strong>'
            f'<span>{blurb}</span></a></li>'
        )
    return "<ul class=\"cards\">\n" + "\n".join(items) + "\n  </ul>"


def _markdown_to_html(md: str) -> str:
    """Render Markdown to HTML.

    Requires the third-party ``markdown`` package; the GitHub Pages workflow
    installs it explicitly with ``pip install markdown`` before invoking this
    script. The package brings in tables, fenced code blocks, sane list
    handling, and newline-to-``<br>`` conversion, which together cover every
    Markdown construct used in ``README.md`` and the other ``docs/*.md``
    files. Importing it at module scope keeps the failure mode obvious: a
    user running ``--landing`` on a fresh laptop without the package gets a
    clear ``ModuleNotFoundError`` instead of silently mangled output.
    """
    import markdown  # type: ignore[import-not-found]

    return markdown.markdown(
        md,
        extensions=["tables", "fenced_code", "sane_lists", "nl2br"],
        output_format="html5",
    )


def write_landing_page(docs_dir: Path, readme_path: Path) -> Path:
    """Render the site landing page from ``readme_path`` into ``docs_dir``.

    Returns the path of the file that was written.
    """
    md = readme_path.read_text(encoding="utf-8")
    body = _markdown_to_html(md)
    cards = _render_landing_cards()
    doc = (
        "<!doctype html>\n"
        '<html lang="en">\n'
        "<head>\n"
        '  <meta charset="utf-8">\n'
        '  <meta name="viewport" content="width=device-width, initial-scale=1">\n'
        "  <title>ledz</title>\n"
        f"  <style>{LANDING_CSS}</style>\n"
        "</head>\n"
        "<body>\n"
        "  <h1>ledz</h1>\n"
        "  <p class=\"tagline\">ESP32-based LED controller with web interface "
        "for WS2812B / NeoPixel strips.</p>\n"
        f"  {cards}\n"
        "  <article>\n"
        f"{body}\n"
        "  </article>\n"
        "</body>\n"
        "</html>\n"
    )
    out = docs_dir / "index.html"
    docs_dir.mkdir(parents=True, exist_ok=True)
    out.write_text(doc, encoding="utf-8")
    print(f"{TAG}: wrote {out}")
    return out


DOC_PAGE_CSS = LANDING_CSS  # same look-and-feel as the landing page


def write_doc_page(docs_dir: Path, md_path: Path) -> Path:
    """Render ``md_path`` to ``{stem}.html`` in ``docs_dir`` with a simple
    navigation bar back to the home page. Used so ``docs/*.md`` files are
    browsable on the GitHub Pages site without serving raw markdown.
    """
    md = md_path.read_text(encoding="utf-8")
    body = _markdown_to_html(md)
    title = html.escape(md_path.stem.replace("_", " "))
    doc = (
        "<!doctype html>\n"
        '<html lang="en">\n'
        "<head>\n"
        '  <meta charset="utf-8">\n'
        '  <meta name="viewport" content="width=device-width, initial-scale=1">\n'
        f"  <title>ledz — {title}</title>\n"
        f"  <style>{DOC_PAGE_CSS}</style>\n"
        "</head>\n"
        "<body>\n"
        '  <p><a href="index.html">&larr; ledz home</a></p>\n'
        "  <article>\n"
        f"{body}\n"
        "  </article>\n"
        "</body>\n"
        "</html>\n"
    )
    out = docs_dir / f"{md_path.stem}.html"
    out.write_text(doc, encoding="utf-8")
    print(f"{TAG}: wrote {out}")
    return out


def write_doc_pages(docs_dir: Path, skip: set[str] = frozenset()) -> list[Path]:
    """Render every ``*.md`` file in ``docs_dir`` (besides ``README.md`` and
    anything in ``skip``) as ``{stem}.html``. The raw ``.md`` files stay in
    place so the GitHub repo still serves them; the generated ``.html`` files
    just make the Pages site browsable.
    """
    written: list[Path] = []
    if not docs_dir.exists():
        return written
    for md_path in sorted(docs_dir.glob("*.md")):
        if md_path.name == "README.md" or md_path.name in skip:
            continue
        written.append(write_doc_page(docs_dir, md_path))
    return written


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="build_pages",
        description="Build the ledz GitHub Pages site: render show previews, "
                    "the show gallery, and the docs landing page.",
    )
    p.add_argument("-n", "--iterations", type=int, default=1000,
                   help="number of rows (iterations); default 1000")
    p.add_argument("-W", "--width", type=int, default=WIDTH,
                   help=f"image width in pixels; default {WIDTH}")
    p.add_argument("-o", "--output", default="show_preview.png",
                   help="output PNG path; default show_preview.png")

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
    p.add_argument("--variants-file", dest="variants_file",
                   default=str(DEFAULT_VARIANTS_FILE),
                   help="path to the show variants config; default "
                        f"{DEFAULT_VARIANTS_FILE}")
    p.add_argument("--all", dest="all_dir", default=None, metavar="DIR",
                   help="render every variant of every registered show into "
                        "DIR and emit DIR/index.html")
    p.add_argument("--landing", dest="landing", action="store_true",
                   help="also render docs/index.html from README.md")
    p.add_argument("--readme", dest="readme_path", default="README.md",
                   help="source for --landing; default README.md")
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
        variants_file=Path(args.variants_file),
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
        if args.landing:
            docs_dir = Path(params.all_dir).parent
            write_landing_page(docs_dir, Path(args.readme_path))
            write_doc_pages(docs_dir)
        return 0

    render_one(params)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
