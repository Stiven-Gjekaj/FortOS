#!/usr/bin/env python3
"""Write src/core/fo_items.c: the items of GANGFORT.

ItemDataHelper (aao.java) lists each item: its id, its kind, the classes
that can use it, its title, its description, and its picture in ui.atlas.
Each hat has its own class (GoldenHat, abn.java, and the others). The
class gives the region or the frames of the hat, its place on the head of
each class, and the polygons of its body when it falls. This tool reads
the decompiled classes in reference/jadx and the sizes of the regions in
sprites.atlas, and writes them as C data.

Usage: tools/items.py [JADX_SOURCES] [SPRITES_ATLAS]
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCES = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "reference/jadx/sources/defpackage")
ATLAS = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, "reference/apk/assets/res/textures/sprites.atlas")
OUT = os.path.join(ROOT, "src/core/fo_items.c")

REGISTER = re.compile(r'^\s*a\((\d+), (\d+), (\w+)\.(\w+), (\w+)\.(\w+), (\w+)\.(\w+), (null|"\w+")\);$')
STRING = re.compile(r'public static final String (\w+) = ald\.a\("(\w+)"\);')
INTS = re.compile(r'public static final int\[\] (\w+) = (?:\{([\d, ]+)\}|wn\.(\w+));')
WN_STRINGS = re.compile(r'public static final String\[\] (\w+) = \{([^}]*)\};')
WN_INTS = re.compile(r'public static final int\[\] (\w+) = \{([\d, ]+)\};')
VECTOR = re.compile(r'if \(i == (\d)\) \{\s*return new Vector2\(([^;]*)\);')
POLYGON = re.compile(r'new float\[\]\{([^}]*)\}')


def read(name):
    with open(os.path.join(SOURCES, name + ".java")) as f:
        return f.read()


def atlas_sizes(path):
    sizes = {}
    name = None
    with open(path) as f:
        for line in f:
            line = line.rstrip("\n")
            if line and not line.startswith(" ") and ":" not in line:
                name = line
            elif name and line.strip().startswith("size:"):
                w, h = line.split(":")[1].split(",")
                sizes[name] = (int(w), int(h))
    return sizes


def number(text):
    # "p() * 3.5f", "4.0f * p()", "(-0.5f) * p()": one art pixel is p().
    expr = text.replace("p()", "1").replace("f", "").strip()
    if not re.fullmatch(r"[0-9.()*\- ]+", expr):
        raise ValueError("cannot read " + text)
    return eval(expr)


def split_args(text):
    out, depth, cur = [], 0, ""
    for ch in text:
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        if ch == "," and depth == 0:
            out.append(cur)
            cur = ""
        else:
            cur += ch
    out.append(cur)
    return [a.strip() for a in out]


def wn_tables():
    src = read("wn")
    strings = {m.group(1): re.findall(r'"(\w+)"', m.group(2)) for m in WN_STRINGS.finditer(src)}
    ints = {m.group(1): [int(x) for x in m.group(2).split(",")] for m in WN_INTS.finditer(src)}
    return strings, ints


def class_fields(src, wn_ints):
    strings = {m.group(1): m.group(2) for m in STRING.finditer(src)}
    ints = {}
    for m in INTS.finditer(src):
        ints[m.group(1)] = [int(x) for x in m.group(2).split(",")] if m.group(2) else wn_ints[m.group(3)]
    return strings, ints


def method(src, signature):
    i = src.find(signature)
    if i < 0:
        return None
    depth, j = 0, src.index("{", i)
    for k in range(j, len(src)):
        if src[k] == "{":
            depth += 1
        elif src[k] == "}":
            depth -= 1
            if depth == 0:
                return src[j:k + 1]
    return None


def hat(name, wn_strings):
    src = read(name)
    parent = re.search(r"extends (\w+)", src).group(1)
    info = hat(parent, wn_strings) if parent != "aan" else {"region": None, "frames": [], "duration": 0.0,
                                                               "loop": False, "offsets": None, "polygons": None}
    info = dict(info)
    call = re.search(r"super\(wtVar, ([^;]*)\);", src)
    this = re.search(r'this\(wtVar, "(\w+)"\);', src)
    if call:
        args = split_args(call.group(1))
        if args == ["str"]:
            info["region"] = this.group(1)
            info["frames"] = []
        elif len(args) == 1:
            info["region"] = args[0].strip('"')
            info["frames"] = []
        else:
            region, frames, duration = args
            info["frames"] = wn_strings[frames.split(".")[1]]
            info["region"] = None if region == "null" else wn_strings[region.split(".")[1].split("[")[0]][0]
            info["duration"] = float(duration.rstrip("f"))
    elif this and parent != "aan":
        info["region"] = this.group(1)
        info["frames"] = []
    if "PlayMode.LOOP" in src:
        info["loop"] = True
    body = method(src, "public Vector2 a(int i)")
    if body:
        offsets = {}
        for m in VECTOR.finditer(body):
            x, y = split_args(m.group(2))
            offsets[int(m.group(1))] = (number(x), number(y))
        info["offsets"] = offsets
    body = method(src, "public float[][] u()")
    if body:
        info["polygons"] = [[float(v.rstrip("f")) for v in m.group(1).split(",")] for m in POLYGON.finditer(body)]
    # The hats that play their frames at times (abn.java, abk.java, abl.java).
    if "this.s != t().U()" in src:
        info["effect"] = "FO_HAT_GOLDEN"
    elif "this.p != t().U()" in src:
        info["effect"] = "FO_HAT_SCORE"
    elif "MathUtils.random(1.0f, 3.0f)" in src:
        info["effect"] = "FO_HAT_SPARKLE"
    return info


def c_string(s):
    return "NULL" if s is None else '"%s"' % s


def main():
    wn_strings, wn_ints = wn_tables()
    sizes = atlas_sizes(ATLAS)
    items = []
    for line in read("aao").splitlines():
        m = REGISTER.match(line)
        if not m:
            continue
        item_id, kind = int(m.group(1)), int(m.group(2))
        cls = m.group(3)
        strings, ints = class_fields(read(cls), wn_ints)
        item = {
            "id": item_id,
            "kind": kind,
            "classes": ints[m.group(4)],
            "title": strings[m.group(6)],
            "description": strings[m.group(8)],
            "icon": None if m.group(9) == "null" else m.group(9).strip('"'),
            "name": re.search(r"compiled from: (\w+)\.java", read(cls)).group(1),
        }
        if kind == 1:
            item.update(hat(cls, wn_strings))
        items.append(item)
    hats = [i for i in items if i["kind"] == 1]
    if len(hats) != 34:
        raise SystemExit("items: %d hats, not 34" % len(hats))
    # The limits of FOItem in fo_items.h. Box2D takes 8 vertices at most.
    for h in hats:
        if len(h["polygons"]) > 4 or max(len(p) // 2 for p in h["polygons"]) > 8:
            raise SystemExit("items: the polygons of %s do not fit FOItem" % h["name"])

    out = []
    out.append("// Written by tools/items.py from ItemDataHelper (aao.java), the hat classes,")
    out.append("// and sprites.atlas of GANGFORT 2.1. Do not edit.")
    out.append("")
    out.append("#include <stddef.h>")
    out.append("")
    out.append('#include "fo_items.h"')
    out.append("")
    for h in hats:
        if h["frames"]:
            out.append("static const char *const kFrames%d[] = {%s};" % (h["id"], ", ".join(c_string(f) for f in h["frames"])))
    out.append("")
    out.append("const FOItem kFOItems[FO_ITEM_COUNT] = {")
    for i in items:
        mask = sum(1 << c for c in i["classes"])
        out.append("    {")
        out.append("        .id = %d, .kind = %d, .classes = 0x%x, .name = %s," % (i["id"], i["kind"], mask, c_string(i["name"])))
        out.append("        .title = %s, .description = %s, .icon = %s," % (c_string(i["title"]), c_string(i["description"]),
                                                                          c_string(i["icon"])))
        if i["kind"] == 1:
            first = i["region"] or i["frames"][0]
            w, h = sizes[first]
            out.append("        .region = %s, .w = %.1ff, .h = %.1ff," % (c_string(i["region"]), w * 0.1, h * 0.1))
            if i["frames"]:
                out.append("        .frames = kFrames%d, .frameCount = %d, .frameTime = %.9gf, .loop = %d," %
                           (i["id"], len(i["frames"]), i["duration"], 1 if i["loop"] else 0))
            if i.get("effect"):
                out.append("        .effect = %s," % i["effect"])
            offsets = i["offsets"]
            row = ["{0, 0}"] + ["{%.2ff, %.2ff}" % (offsets[c][0] * 0.1, offsets[c][1] * 0.1) for c in range(1, 10)]
            out.append("        .offsets = {%s}," % ", ".join(row))
            polys = i["polygons"]
            out.append("        .polygonCount = %d," % len(polys))
            out.append("        .polygons = {%s}," % ", ".join(
                "{%s}" % ", ".join("%.2ff" % (v * 0.1) for v in p) for p in polys))
            out.append("        .polygonSizes = {%s}," % ", ".join(str(len(p) // 2) for p in polys))
        out.append("    },")
    out.append("};")
    out.append("")
    with open(OUT, "w") as f:
        f.write("\n".join(out))
    print("%s: %d items, %d hats" % (OUT, len(items), len(hats)))


if __name__ == "__main__":
    main()
