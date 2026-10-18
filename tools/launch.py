#!/usr/bin/env python3
"""Write resources/Default.png and resources/Default@2x.png.

iOS 6 shows the launch image while the app starts. These images have the
colour of the background of SplashScreen (ahe.java:79): (0.1484375,
0.13671875, 0.1328125), or #262322. So the start goes from this colour to
the splash with no black screen. The images of an iPhone app are portrait.

Usage: tools/launch.py
"""

import os
import struct
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
COLOUR = bytes([0x26, 0x23, 0x22])


def png(path, width, height):
    row = b"\x00" + COLOUR * width
    data = zlib.compress(row * height, 9)

    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)))
        f.write(chunk(b"IDAT", data))
        f.write(chunk(b"IEND", b""))


png(os.path.join(ROOT, "resources/Default.png"), 320, 480)
png(os.path.join(ROOT, "resources/Default@2x.png"), 640, 960)
print("resources/Default.png, resources/Default@2x.png")
