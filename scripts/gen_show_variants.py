#!/usr/bin/env python3
"""
Build script that emits src/generated/show_variants.h from scripts/show_variants.json.

The header declares a `constexpr` table of every show's name, description,
default-params JSON, and curated variant list. The firmware's ShowFactory
includes the header and uses it as the runtime source of default parameters;
the touch controller's Solid single-colour entries are also sourced from the
header instead of a hand-coded C++ array.

Can be run standalone or as a PlatformIO pre-build script.
"""

import json
import re
import sys
from pathlib import Path

# Handle PlatformIO environment
try:
    Import("env")
    PLATFORMIO_BUILD = True
    PROJECT_DIR = Path(env.subst("$PROJECT_DIR"))
except Exception:
    PLATFORMIO_BUILD = False
    SCRIPT_DIR = Path(__file__).parent
    PROJECT_DIR = SCRIPT_DIR.parent


VARIANTS_FILE = PROJECT_DIR / "scripts" / "show_variants.json"
OUTPUT_FILE = PROJECT_DIR / "src" / "generated" / "show_variants.h"
JS_OUTPUT_FILE = PROJECT_DIR / "src" / "generated" / "show_variants.js"


def cpp_string_literal(s: str) -> str:
    """Encode a Python string as a C++ raw-string literal using R"(...)".

    Uses the delim form R"d(...)d" where d is empty (or padded if the body
    itself contains ")") so the literal never collides with the body.
    """
    if not s:
        return 'R"()"' if False else '""'  # empty -> ""
    delim = ""
    while (")" + delim + "\"") in s:
        delim += "="
    return f'R"{delim}({s}){delim}"'


def identifier(name: str) -> str:
    """Map a show name to a valid C++ identifier suffix for variant arrays."""
    out = []
    for ch in name:
        if ch.isalnum():
            out.append(ch)
        else:
            out.append("_")
    return "".join(out)


def normalize_for_arduinojson(value):
    """Mimic ArduinoJson 7's serialisation: drop trailing ``.0`` from floats
    that fit in an int (``4.0`` → ``4``, ``10.0`` → ``10``; ``0.5`` stays).

    This keeps the generated header's ``default_params_json`` byte-equal to
    the firmware's `serializeJson` output for the same data, which is what the
    parity test asserts.
    """
    if isinstance(value, float) and value.is_integer():
        return int(value)
    if isinstance(value, list):
        return [normalize_for_arduinojson(v) for v in value]
    if isinstance(value, dict):
        return {k: normalize_for_arduinojson(v) for k, v in value.items()}
    return value


FLAG_NAMES = ("ukraine", "italy", "france")


def build_js_payload(shows):
    """Emit the JS payload consumed by the web UI.

    Two top-level ``const`` declarations:

    * ``SHOW_VARIANTS_BY_SHOW``: keys are show names; values are objects whose
      keys are the synthetic ``"default"`` plus every variant name, and whose
      values are ``{ "label": <label>, "params": <params> }`` objects — the
      manifest label (``"Default"`` for the synthetic default entry) alongside
      the corresponding ``params`` object (after ``normalize_for_arduinojson``).
    * ``FLAG_PRESETS``: keys are the three flag names; values are the
      corresponding ``Solid.variants[]`` entries' plain ``params`` objects.
    """
    by_show = {}
    for show in shows:
        entries = {"default": {"label": "Default", "params": json.loads(show["default_json"])}}
        for v in show["variants"]:
            entries[v["name"]] = {"label": v["label"], "params": json.loads(v["params_json"])}
        by_show[show["name"]] = entries

    flag_presets = {}
    for show in shows:
        if show["name"] != "Solid":
            continue
        for v in show["variants"]:
            if v["name"] in FLAG_NAMES:
                flag_presets[v["name"]] = json.loads(v["params_json"])
        break

    js_indent_separators = (",", ": ")
    return (
        "const SHOW_VARIANTS_BY_SHOW = "
        f"{json.dumps(by_show, separators=js_indent_separators, ensure_ascii=False)};\n"
        "const FLAG_PRESETS = "
        f"{json.dumps(flag_presets, separators=js_indent_separators, ensure_ascii=False)};\n"
    )


def main() -> int:
    if not VARIANTS_FILE.exists():
        print(f"gen_show_variants: {VARIANTS_FILE} not found", file=sys.stderr)
        return 1

    try:
        with VARIANTS_FILE.open("r", encoding="utf-8") as f:
            raw = json.load(f)
    except json.JSONDecodeError as e:
        print(f"gen_show_variants: {VARIANTS_FILE} is not valid JSON: {e}", file=sys.stderr)
        return 1

    shows = []
    for show_name, body in raw.items():
        if show_name.startswith("_"):
            continue
        if not isinstance(body, dict):
            print(f"gen_show_variants: show {show_name!r} has non-dict body", file=sys.stderr)
            return 1
        description = body.get("description", "")
        default_obj = body.get("default", {})
        if not isinstance(default_obj, dict):
            print(f"gen_show_variants: show {show_name!r} has non-dict default", file=sys.stderr)
            return 1
        default_params = default_obj.get("params", {})
        variants_in = body.get("variants", [])
        if not isinstance(variants_in, list):
            print(f"gen_show_variants: show {show_name!r} has non-list variants", file=sys.stderr)
            return 1
        default_params_norm = normalize_for_arduinojson(default_params)
        default_json = json.dumps(default_params_norm, separators=(",", ":"), ensure_ascii=False)
        variants = []
        for v in variants_in:
            if not isinstance(v, dict):
                print(f"gen_show_variants: show {show_name!r} has non-dict variant", file=sys.stderr)
                return 1
            v_name = v.get("name")
            v_label = v.get("label", v_name)
            v_params = v.get("params", {})
            if not isinstance(v_name, str):
                print(f"gen_show_variants: show {show_name!r} variant missing string name", file=sys.stderr)
                return 1
            variants.append({
                "name": v_name,
                "label": v_label,
                "params_json": json.dumps(
                    normalize_for_arduinojson(v_params),
                    separators=(",", ":"),
                    ensure_ascii=False,
                ),
            })
        shows.append({
            "name": show_name,
            "description": description,
            "default_json": default_json,
            "variants": variants,
        })

    shows.sort(key=lambda s: s["name"])

    OUTPUT_FILE.parent.mkdir(parents=True, exist_ok=True)

    lines = []
    lines.append("// Auto-generated file - do not edit manually")
    lines.append("// Generated by scripts/gen_show_variants.py from scripts/show_variants.json")
    lines.append("")
    lines.append("#ifndef SHOW_VARIANTS_H")
    lines.append("#define SHOW_VARIANTS_H")
    lines.append("")
    lines.append("#include <cstddef>")
    lines.append("#include <cstring>")
    lines.append("")
    lines.append("namespace ShowVariants {")
    lines.append("")
    lines.append("struct Variant {")
    lines.append("    const char* name;")
    lines.append("    const char* label;")
    lines.append("    const char* params_json;")
    lines.append("};")
    lines.append("")
    lines.append("struct ShowEntry {")
    lines.append("    const char* name;")
    lines.append("    const char* description;")
    lines.append("    const char* default_params_json;")
    lines.append("    const Variant* variants;")
    lines.append("    std::size_t num_variants;")
    lines.append("};")
    lines.append("")

    for show in shows:
        if show["variants"]:
            array_name = f"k{identifier(show['name'])}Variants"
            lines.append(f"constexpr Variant {array_name}[] = {{")
            for v in show["variants"]:
                lines.append(
                    f"    {{{cpp_string_literal(v['name'])}, "
                    f"{cpp_string_literal(v['label'])}, "
                    f"{cpp_string_literal(v['params_json'])}}},"
                )
            lines.append("};")
            lines.append("")

    lines.append(f"constexpr std::size_t kNumShows = {len(shows)};")
    lines.append(f"constexpr ShowEntry kShows[kNumShows] = {{")
    for show in shows:
        array_name = f"k{identifier(show['name'])}Variants"
        if show["variants"]:
            count_expr = f"(sizeof({array_name}) / sizeof({array_name}[0]))"
            variants_expr = array_name
        else:
            count_expr = "0"
            variants_expr = "nullptr"
        lines.append(
            f"    {{"
            f"{cpp_string_literal(show['name'])}, "
            f"{cpp_string_literal(show['description'])}, "
            f"{cpp_string_literal(show['default_json'])}, "
            f"{variants_expr}, "
            f"{count_expr}"
            f"}},"
        )
    lines.append("};")
    lines.append("")

    lines.append("inline const ShowEntry* findByName(const char* name) {")
    lines.append("    if (name == nullptr) return nullptr;")
    lines.append("    for (std::size_t i = 0; i < kNumShows; ++i) {")
    lines.append("        if (std::strcmp(kShows[i].name, name) == 0) {")
    lines.append("            return &kShows[i];")
    lines.append("        }")
    lines.append("    }")
    lines.append("    return nullptr;")
    lines.append("}")
    lines.append("")
    lines.append("inline const char* findDefaultParamsJson(const char* name) {")
    lines.append("    const ShowEntry* entry = findByName(name);")
    lines.append("    return entry ? entry->default_params_json : nullptr;")
    lines.append("}")
    lines.append("")
    lines.append("inline const Variant* findVariants(const char* name, std::size_t* out_num_variants) {")
    lines.append("    const ShowEntry* entry = findByName(name);")
    lines.append("    if (entry == nullptr) {")
    lines.append("        if (out_num_variants) *out_num_variants = 0;")
    lines.append("        return nullptr;")
    lines.append("    }")
    lines.append("    if (out_num_variants) *out_num_variants = entry->num_variants;")
    lines.append("    return entry->variants;")
    lines.append("}")
    lines.append("")
    lines.append("} // namespace ShowVariants")
    lines.append("")
    lines.append("#endif // SHOW_VARIANTS_H")
    lines.append("")

    OUTPUT_FILE.write_text("\n".join(lines), encoding="utf-8")
    print(f"gen_show_variants: wrote {OUTPUT_FILE} ({len(shows)} shows)")

    js_payload = build_js_payload(shows)
    JS_OUTPUT_FILE.write_text(js_payload, encoding="utf-8")
    print(f"gen_show_variants: wrote {JS_OUTPUT_FILE}")
    return 0


if __name__ == "__main__" or PLATFORMIO_BUILD:
    main()
