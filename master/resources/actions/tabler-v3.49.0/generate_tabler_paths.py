#!/usr/bin/env python3
"""generate_tabler_paths.py - NextSpace Teacher action-icon path-data generator (V3an GC2-S1, V3ao GC2-S2a,
V3ap GC2-S2b, V3aq GC2-S2c, V3ar GC2-S2d, V3at GC2-S2f, V3aw GC2-S2i, V3ax, V3bb, V3bd, V3bh, V3bi)

Reads the 58 pinned Tabler Icons outline sources in this directory, verifies each one against its manifest.tsv
SHA-256, parses only the SVG subset these sources use (the fixed Tabler <svg> root and <path d="..."> elements
with the commands M m l h v c a) and writes master/src/TeacherActionIconData.h. Coordinates become absolute
24x24 view-box values; elliptical arcs become cubic Bezier segments; every value is rounded to 4 decimals. Any
other element, attribute, command or file is rejected, never ignored. Standard library only; deterministic.

usage (from the veyon source directory):
  py master/resources/actions/tabler-v3.49.0/generate_tabler_paths.py            write the header
  py master/resources/actions/tabler-v3.49.0/generate_tabler_paths.py --check    verify the committed header
  py master/resources/actions/tabler-v3.49.0/generate_tabler_paths.py --output PATH
"""
import argparse
import hashlib
import math
import re
import sys
import xml.etree.ElementTree as ElementTree
from pathlib import Path

FAMILY = "Tabler Icons outline"
TAG = "v3.49.0"
COMMIT = "bbed884d15354b5cebf2493371f20dc2d5e83eaf"
GLYPHS = ("app-window", "apps", "camera", "device-desktop", "dots", "eye", "file-download", "file-upload", "lock",
          "lock-open", "login", "logout", "maximize", "message", "pencil", "pointer", "power", "refresh",
          "screen-share", "world",
          # V3ao GC2-S2a, appended after the twenty reviewed V3an glyphs
          "devices-pc", "slideshow", "lamp-2", "users", "zoom-scan", "layout-grid", "align-box-center-middle",
          "folder-open", "device-floppy", "info-circle",
          # V3ap GC2-S2b, appended after the thirty reviewed glyphs
          "trash",
          # V3aq GC2-S2c, appended after the thirty-one reviewed glyphs
          "player-skip-back", "player-skip-forward",
          # V3ar GC2-S2d, appended after the thirty-three reviewed glyphs
          "player-play", "player-pause",
          # V3at GC2-S2f, appended after the thirty-five reviewed glyphs
          "device-desktop-plus", "device-desktop-minus", "refresh-off",
          # V3aw GC2-S2i, appended after the thirty-eight reviewed glyphs
          "keyboard", "x",
          # V3ax, appended after the forty reviewed glyphs
          "clock", "transfer", "hourglass", "circle-check",
          # V3bb Administration page identities, appended after the forty-four reviewed glyphs
          "settings", "presentation", "shield-lock", "key", "brand-windows", "hierarchy-2",
          # V3bd Administration Access Control commands, appended after the fifty reviewed glyphs
          "arrow-right", "arrow-left", "plus", "arrow-up", "arrow-down",
          # V3bh Administration Student Agent Stop service command, appended after the fifty-five reviewed glyphs
          "player-stop",
          # V3bi service-control Toast Warning and Error icon masks, appended after the fifty-six reviewed glyphs
          "alert-triangle", "circle-x")
BUNDLE_DIR = "master/resources/actions/tabler-v3.49.0"
COMMAND = f"py {BUNDLE_DIR}/generate_tabler_paths.py"
HERE = Path(__file__).resolve().parent
HEADER = HERE.parents[3] / "master" / "src" / "TeacherActionIconData.h"
BUNDLE_FILES = {f"{g}.svg" for g in GLYPHS} | {"LICENSE", "manifest.tsv", "README.md", "generate_tabler_paths.py"}

MANIFEST_FIELDS = ["family", "tag", "commit", "upstream_path", "local_path", "sha256", "upstream_stroke",
                   "local_stroke", "consumers", "treatment", "modification"]
SVG_NAMESPACE = "{http://www.w3.org/2000/svg}"
ROOT_ATTRIBUTES = {"width": "24", "height": "24", "viewBox": "0 0 24 24", "fill": "none", "stroke": "currentColor",
                   "stroke-width": "2", "stroke-linecap": "round", "stroke-linejoin": "round"}
SOURCE_STROKE = "2"
RENDER_STROKE = "2.1"
# commands used by the 58 sources and their argument counts; anything else is unsupported
COMMANDS = {"M": 2, "m": 2, "l": 2, "h": 1, "v": 1, "c": 6, "a": 7}
TOKEN = re.compile(r"\s*(?:([A-Za-z])|([-+]?(?:\d+\.\d*|\.\d+|\d+)(?:[eE][-+]?\d+)?))")


class GeneratorError(Exception):
    pass


def source_digest(data):
    """SHA-256 of the source bytes; a CRLF working-tree checkout of the LF-only upstream blob hashes identically."""
    return hashlib.sha256(data.replace(b"\r\n", b"\n")).hexdigest()


def parse_manifest(text):
    lines = [line for line in text.replace("\r\n", "\n").split("\n") if line.strip() and not line.startswith("#")]
    if not lines or lines[0].split("\t") != MANIFEST_FIELDS:
        raise GeneratorError("manifest.tsv has an unexpected header")
    rows = {}
    for line in lines[1:]:
        values = line.split("\t")
        if len(values) != len(MANIFEST_FIELDS):
            raise GeneratorError(f"manifest row has {len(values)} fields: {line[:60]}")
        row = dict(zip(MANIFEST_FIELDS, values))
        name = row["local_path"].rsplit("/", 1)[-1].removesuffix(".svg")
        if name not in GLYPHS or name in rows:
            raise GeneratorError(f"manifest names an unapproved or duplicate glyph '{name}'")
        if (row["family"], row["tag"], row["commit"]) != (FAMILY, TAG, COMMIT):
            raise GeneratorError(f"manifest row '{name}' is not pinned to {FAMILY} {TAG} {COMMIT}")
        if row["upstream_path"] != f"icons/outline/{name}.svg" or row["local_path"] != f"{BUNDLE_DIR}/{name}.svg":
            raise GeneratorError(f"manifest row '{name}' has unexpected paths")
        if not re.fullmatch(r"[0-9a-f]{64}", row["sha256"]):
            raise GeneratorError(f"manifest row '{name}' has no SHA-256")
        if (row["upstream_stroke"], row["local_stroke"]) != (SOURCE_STROKE, RENDER_STROKE):
            raise GeneratorError(f"manifest row '{name}' records unexpected strokes")
        rows[name] = row
    if sorted(rows) != sorted(GLYPHS):
        raise GeneratorError(f"manifest lacks {sorted(set(GLYPHS) - set(rows))}")
    return rows


def tokenize(d):
    tokens = []
    pos = 0
    while pos < len(d):
        match = TOKEN.match(d, pos)
        if match is None or match.end() == pos:
            if d[pos:].strip() == "":
                break
            raise GeneratorError(f"unparseable path data at '{d[pos:pos + 12]}'")
        if match.group(1):
            tokens.append(("cmd", match.group(1)))
        else:
            tokens.append(("num", match.group(2)))
        pos = match.end()
    return tokens


def arc_to_cubics(x1, y1, rx, ry, rotation, large, sweep, x2, y2):
    """SVG 1.1 F.6.5 endpoint-to-centre conversion, then at most 90-degree cubic segments."""
    if (x1, y1) == (x2, y2):
        return []
    rx, ry = abs(rx), abs(ry)
    if rx == 0 or ry == 0:
        return [("L", x2, y2)]
    phi = math.radians(rotation)
    cos_phi, sin_phi = math.cos(phi), math.sin(phi)
    dx, dy = (x1 - x2) / 2, (y1 - y2) / 2
    x1p = cos_phi * dx + sin_phi * dy
    y1p = -sin_phi * dx + cos_phi * dy
    radii_check = x1p * x1p / (rx * rx) + y1p * y1p / (ry * ry)
    if radii_check > 1:
        rx, ry = rx * math.sqrt(radii_check), ry * math.sqrt(radii_check)
    numerator = rx * rx * ry * ry - rx * rx * y1p * y1p - ry * ry * x1p * x1p
    denominator = rx * rx * y1p * y1p + ry * ry * x1p * x1p
    coefficient = math.sqrt(max(0.0, numerator / denominator))
    if large == sweep:
        coefficient = -coefficient
    cxp = coefficient * rx * y1p / ry
    cyp = -coefficient * ry * x1p / rx
    cx = cos_phi * cxp - sin_phi * cyp + (x1 + x2) / 2
    cy = sin_phi * cxp + cos_phi * cyp + (y1 + y2) / 2

    def angle(ux, uy, vx, vy):
        return math.atan2(ux * vy - uy * vx, ux * vx + uy * vy)

    theta = angle(1, 0, (x1p - cxp) / rx, (y1p - cyp) / ry)
    delta = angle((x1p - cxp) / rx, (y1p - cyp) / ry, (-x1p - cxp) / rx, (-y1p - cyp) / ry)
    if not sweep and delta > 0:
        delta -= 2 * math.pi
    elif sweep and delta < 0:
        delta += 2 * math.pi
    count = max(1, math.ceil(abs(delta) / (math.pi / 2) - 1e-9))
    step = delta / count
    handle = 4 / 3 * math.tan(step / 4)

    def point(t):
        return (cx + rx * math.cos(t) * cos_phi - ry * math.sin(t) * sin_phi,
                cy + rx * math.cos(t) * sin_phi + ry * math.sin(t) * cos_phi)

    def derivative(t):
        return (-rx * math.sin(t) * cos_phi - ry * math.cos(t) * sin_phi,
                -rx * math.sin(t) * sin_phi + ry * math.cos(t) * cos_phi)

    segments = []
    for index in range(count):
        t1, t2 = theta + index * step, theta + (index + 1) * step
        (p1x, p1y), (p2x, p2y) = point(t1), point(t2)
        (d1x, d1y), (d2x, d2y) = derivative(t1), derivative(t2)
        if index == count - 1:
            p2x, p2y = x2, y2
        segments.append(("C", p1x + handle * d1x, p1y + handle * d1y, p2x - handle * d2x, p2y - handle * d2y,
                         p2x, p2y))
    return segments


def parse_path(name, d):
    tokens = tokenize(d)
    if not tokens or tokens[0] != ("cmd", "M"):
        raise GeneratorError(f"{name}: path data must start with an absolute M")
    segments = []
    x = y = 0.0
    index = 0
    command = None
    while index < len(tokens):
        kind, value = tokens[index]
        if kind == "cmd":
            if value not in COMMANDS:
                raise GeneratorError(f"{name}: unsupported path command '{value}'")
            command = value
            index += 1
        elif command is None:
            raise GeneratorError(f"{name}: number without a command")
        count = COMMANDS[command]
        args = tokens[index:index + count]
        if len(args) != count or any(k != "num" for k, _ in args):
            raise GeneratorError(f"{name}: command '{command}' needs {count} numbers")
        raw = [v for _, v in args]
        n = [float(v) for v in raw]
        index += count
        if command == "M":
            x, y = n
            segments.append(("M", x, y))
            # implicit coordinates after an absolute move would be an absolute line-to, outside the subset
            command = None
        elif command == "m":
            x, y = x + n[0], y + n[1]
            segments.append(("M", x, y))
            command = "l"
        elif command == "l":
            x, y = x + n[0], y + n[1]
            segments.append(("L", x, y))
        elif command == "h":
            x += n[0]
            segments.append(("L", x, y))
        elif command == "v":
            y += n[0]
            segments.append(("L", x, y))
        elif command == "c":
            segments.append(("C", x + n[0], y + n[1], x + n[2], y + n[3], x + n[4], y + n[5]))
            x, y = x + n[4], y + n[5]
        elif command == "a":
            if raw[3] not in ("0", "1") or raw[4] not in ("0", "1"):
                raise GeneratorError(f"{name}: arc flags must be 0 or 1")
            end_x, end_y = x + n[5], y + n[6]
            segments += arc_to_cubics(x, y, n[0], n[1], n[2], raw[3] == "1", raw[4] == "1", end_x, end_y)
            x, y = end_x, end_y
    return segments


def parse_svg(name, data):
    try:
        text = data.replace(b"\r\n", b"\n").decode("utf-8")
    except UnicodeDecodeError as error:
        raise GeneratorError(f"{name}: not UTF-8") from error
    if "<!DOCTYPE" in text or "<!ENTITY" in text:
        raise GeneratorError(f"{name}: DTDs and entities are not supported")
    header = re.match(r"<!--\n((?:(?:tags|category|version|unicode): [^\n]*\n)+)-->\n", text)
    keys = [line.split(":", 1)[0] for line in header.group(1).splitlines()] if header else []
    if header is None or len(keys) != len(set(keys)) or not {"tags", "unicode"} <= set(keys):
        raise GeneratorError(f"{name}: not a Tabler outline source (missing the Tabler metadata comment)")
    try:
        root = ElementTree.fromstring(text)
    except ElementTree.ParseError as error:
        raise GeneratorError(f"{name}: malformed SVG: {error}") from error
    if root.tag != f"{SVG_NAMESPACE}svg" or dict(root.attrib) != ROOT_ATTRIBUTES:
        raise GeneratorError(f"{name}: unexpected <svg> root or attributes {dict(root.attrib)}")
    if (root.text or "").strip():
        raise GeneratorError(f"{name}: unexpected text in <svg>")
    segments = []
    for element in root:
        if element.tag != f"{SVG_NAMESPACE}path":
            raise GeneratorError(f"{name}: unsupported element {element.tag.removeprefix(SVG_NAMESPACE)}")
        if set(element.attrib) != {"d"} or len(element) or (element.text or "").strip() or (element.tail or "").strip():
            raise GeneratorError(f"{name}: unsupported <path> attributes or content {sorted(element.attrib)}")
        segments += parse_path(name, element.attrib["d"])
    if not segments:
        raise GeneratorError(f"{name}: no geometry")
    return segments


def number(value):
    value = round(value, 4)
    if value == 0:
        value = 0.0
    text = f"{value:.4f}".rstrip("0")
    return (text + "0" if text.endswith(".") else text) + "f"


def identifier(name):
    words = name.split("-")
    return words[0] + "".join(w.capitalize() for w in words[1:])


def enumerator(name):
    return "".join(w.capitalize() for w in name.split("-"))


def wrap(items, per_line):
    return ["\t" + ", ".join(items[i:i + per_line]) + "," for i in range(0, len(items), per_line)]


def render_header(manifest_text, sources):
    rows = parse_manifest(manifest_text)
    if sorted(sources) != sorted(GLYPHS):
        raise GeneratorError(f"source set differs from the {len(GLYPHS)} approved glyphs: {sorted(set(sources) ^ set(GLYPHS))}")
    glyph_segments = {}
    for name in GLYPHS:
        if source_digest(sources[name]) != rows[name]["sha256"]:
            raise GeneratorError(f"{name}.svg does not match its manifest SHA-256")
        glyph_segments[name] = parse_svg(name, sources[name])

    ops = {"M": "Op::MoveTo", "L": "Op::LineTo", "C": "Op::CubicTo"}
    out = [
        "// TeacherActionIconData.h - Tabler Icons outline path data for the NextSpace Teacher action icons",
        "//",
        f"// Generated by {BUNDLE_DIR}/generate_tabler_paths.py. Do not edit.",
        f"// Regenerate: {COMMAND}",
        f"// Verify:     {COMMAND} --check",
        "//",
        f"// Family: {FAMILY}, tag {TAG}, commit {COMMIT}",
        "// Licence: MIT, Copyright (c) 2020-2026 Paweł Kuna; see THIRD-PARTY-NOTICES.txt and",
        f"//          {BUNDLE_DIR}/LICENSE",
        "// Inputs (SHA-256):",
    ]
    out += [f"//   {name}.svg {rows[name]['sha256']}" for name in GLYPHS]
    out += [
        "//",
        "// Geometry: absolute 24x24 view-box coordinates of the unmodified sources. Relative commands are resolved",
        "// and elliptical arcs are converted to cubic Bezier segments (at most 90 degrees each); values are rounded",
        f"// to 4 decimals. Upstream stroke width {SOURCE_STROKE}; NextSpace renders with {RENDER_STROKE} view-box units.",
        "",
        "#pragma once",
        "",
        "namespace TeacherActionIconData",
        "{",
        "",
        "enum class Glyph : int",
        "{",
    ]
    out += [f"\t{enumerator(name)}," for name in GLYPHS]
    out += [
        "\tCount",
        "};",
        "",
        "enum class Op : unsigned char",
        "{",
        "\tMoveTo, // x, y",
        "\tLineTo, // x, y",
        "\tCubicTo // control 1 x, y, control 2 x, y, end x, y",
        "};",
        "",
        "inline constexpr float ViewBoxSize = 24.0f;",
        f"inline constexpr float SourceStrokeWidth = {number(float(SOURCE_STROKE))};",
        f"inline constexpr float RenderStrokeWidth = {number(float(RENDER_STROKE))};",
        "",
        "struct GlyphPath",
        "{",
        "\tconst char* name;",
        "\tconst Op* ops;",
        "\tint opCount;",
        "\tconst float* coords;",
        "\tint coordCount;",
        "};",
        "",
        "namespace Detail",
        "{",
    ]
    table = []
    for name in GLYPHS:
        segments = glyph_segments[name]
        coords = [number(v) for segment in segments for v in segment[1:]]
        out += ["", f"// {name}.svg", f"inline constexpr Op {identifier(name)}Ops[] = {{"]
        out += wrap([ops[s[0]] for s in segments], 6)
        out += ["};", f"inline constexpr float {identifier(name)}Coords[] = {{"]
        out += wrap(coords, 8)
        out += ["};"]
        table.append(f"\t{{ \"{name}\", Detail::{identifier(name)}Ops, {len(segments)}, "
                     f"Detail::{identifier(name)}Coords, {len(coords)} }},")
    out += [
        "",
        "}",
        "",
        "// indexed by Glyph",
        "inline constexpr GlyphPath glyphPaths[] = {",
    ]
    out += table
    out += [
        "};",
        "",
        "static_assert( sizeof(glyphPaths) / sizeof(glyphPaths[0]) == static_cast<unsigned int>( Glyph::Count ),",
        "\t\t\t   \"one path per glyph\" );",
        "",
        "}",
        "",
    ]
    return "\n".join(out)


def read_bundle(directory):
    unexpected = sorted(p.name for p in directory.iterdir() if p.name not in BUNDLE_FILES and p.name != "__pycache__")
    if unexpected:
        raise GeneratorError(f"unapproved files in the source bundle: {unexpected}")
    manifest = (directory / "manifest.tsv").read_bytes().decode("utf-8")
    sources = {}
    for name in GLYPHS:
        path = directory / f"{name}.svg"
        if not path.is_file():
            raise GeneratorError(f"{name}.svg is missing")
        sources[name] = path.read_bytes()
    return manifest, sources


def main(argv):
    parser = argparse.ArgumentParser(description="Generate the NextSpace Teacher action-icon path data.")
    parser.add_argument("--check", action="store_true", help="verify the committed header instead of writing it")
    parser.add_argument("--output", type=Path, help="write the header to this path instead")
    args = parser.parse_args(argv)
    try:
        header = render_header(*read_bundle(HERE))
    except (GeneratorError, OSError) as error:
        print(f"generate_tabler_paths: {error}", file=sys.stderr)
        return 2
    if args.check:
        committed = HEADER.read_bytes().replace(b"\r\n", b"\n").decode("utf-8") if HEADER.is_file() else None
        if committed != header:
            print(f"generate_tabler_paths: {HEADER.name} does not match the pinned sources; regenerate it",
                  file=sys.stderr)
            return 1
        print(f"generate_tabler_paths: {HEADER.name} matches {len(GLYPHS)} pinned {FAMILY} {TAG} sources")
        return 0
    target = args.output or HEADER
    target.write_bytes(header.encode("utf-8"))
    print(f"generate_tabler_paths: wrote {target.name} "
          f"(SHA-256 {hashlib.sha256(header.encode('utf-8')).hexdigest()})")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
