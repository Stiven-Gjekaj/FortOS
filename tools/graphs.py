#!/usr/bin/env python3
"""Write src/core/fo_graphs.c: the waypoint graphs of the bots.

GANGFORT keeps the graph of each map in the code of the map class: the
inner class "a extends zq" of acj.java (bomb_canyon), aco.java
(flag_egypt), and acp.java (gts_gantown). This tool reads the decompiled
classes in reference/jadx and writes their calls as C data.

Usage: tools/graphs.py [JADX_SOURCES]
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCES = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "reference/jadx/sources/defpackage")
OUT = os.path.join(ROOT, "src/core/fo_graphs.c")

MAPS = [("canyon", "acj.java"), ("egypt", "aco.java"), ("gantown", "acp.java")]
TYPES = ["Walk", "Jump", "JumpWait100", "JumpWait300", "DoubleJump1000", "DoubleJump500",
         "DoubleJumpMidairOnly700"]

POINT = re.compile(r"^\s*a\((\d+), (-?[0-9.E-]+)f, (-?[0-9.E-]+)f\);$")
LINK = re.compile(r"^\s*a\((\d+), (\d+), aac\.(\w+)(?:, aac\.(\w+))?\);$")
CAMP = re.compile(r"^\s*a\((\d+), (\d+), (true|false), (true|false)\);$")


def compare(points, a, b):
    # zr.java: the camp spots go in the order of x. Equal x gives -1.
    return 1 if points[a[0]][0] > points[b[0]][0] else -1


def timsort_small(items, cmp):
    """The TimSort of libGDX for fewer than 32 items: a run, then a binary
    insertion sort. Array.sort runs after each new camp spot (zq.java),
    and the order of equal items depends on this algorithm."""
    n = len(items)
    if n < 2:
        return
    run = 1
    if run < n:
        if cmp(items[1], items[0]) < 0:
            run = 2
            while run < n and cmp(items[run], items[run - 1]) < 0:
                run += 1
            items[0:run] = items[0:run][::-1]
        else:
            run = 2
            while run < n and cmp(items[run], items[run - 1]) >= 0:
                run += 1
    start = run if run > 0 else 1
    for i in range(start, n):
        pivot = items[i]
        left, right = 0, i
        while left < right:
            mid = (left + right) >> 1
            if cmp(pivot, items[mid]) < 0:
                right = mid
            else:
                left = mid + 1
        items[left + 1:i + 1] = items[left:i]
        items[left] = pivot


def read_map(path):
    text = open(path).read()
    start = text.index("class a extends zq")
    body = text[start:]
    end = body.index("@Override")
    points, links, camps = {}, [], []
    order = []
    for line in body[:end].splitlines():
        m = POINT.match(line)
        if m:
            pid = int(m.group(1))
            points[pid] = (m.group(2), m.group(3))
            order.append(pid)
            continue
        m = LINK.match(line)
        if m:
            back = TYPES.index(m.group(4)) if m.group(4) else -1
            links.append((int(m.group(1)), int(m.group(2)), TYPES.index(m.group(3)), back))
            continue
        m = CAMP.match(line)
        if m:
            camps.append((int(m.group(1)), int(m.group(2)), m.group(3) == "true", m.group(4) == "true"))
            xs = {k: (float(v[0]), float(v[1])) for k, v in points.items()}
            timsort_small(camps, lambda a, b: compare(xs, a, b))
            continue
        if re.match(r"^\s*a\(", line):
            raise SystemExit("unknown call in %s: %s" % (path, line.strip()))
    return order, points, links, camps


def main():
    out = []
    out.append("// fo_graphs: the waypoint graphs of the bots on the three maps.")
    out.append("// tools/graphs.py writes this file from the map classes of GANGFORT 2.1")
    out.append("// (the inner class a of acj.java, aco.java, and acp.java). Do not edit it.")
    out.append("")
    out.append('#include "fo_graph.h"')
    out.append("")
    names = []
    for name, java in MAPS:
        order, points, links, camps = read_map(os.path.join(SOURCES, java))
        cap = name.capitalize()
        out.append("static const FOGraphPoint k%sPoints[] = {" % cap)
        for pid in order:
            x, y = points[pid]
            out.append("    {%d, %sf, %sf}," % (pid, x, y))
        out.append("};")
        out.append("")
        out.append("static const FOGraphLink k%sLinks[] = {" % cap)
        for a, b, ab, ba in links:
            out.append("    {%d, %d, %d, %d}," % (a, b, ab, ba))
        out.append("};")
        out.append("")
        out.append("static const FOGraphCamp k%sCamps[] = {" % cap)
        for wp, team, c, d in camps:
            out.append("    {%d, %d, %d, %d}," % (wp, team, int(c), int(d)))
        out.append("};")
        out.append("")
        names.append((name, cap, len(order), len(links), len(camps)))
    for name, cap, np, nl, nc in names:
        out.append("const FOGraphData fo_graph_%s = {k%sPoints, %d, k%sLinks, %d, k%sCamps, %d};"
                   % (name, cap, np, cap, nl, cap, nc))
    open(OUT, "w").write("\n".join(out) + "\n")
    for name, cap, np, nl, nc in names:
        print("%s: %d waypoints, %d link calls, %d camp spots" % (name, np, nl, nc))


if __name__ == "__main__":
    main()
