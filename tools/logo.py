# Make the logo of FortOS from assets/brand/logo-source.png.
# It removes the dark background, keeps the circle and the character, and
# adds a gloss over the top of the circle. It also makes the icons of the
# app: the logo on the dark blue of the source, because iOS 6 shows a
# transparent icon pixel as black.
# Usage: python3 tools/logo.py
# It needs Pillow, NumPy, and SciPy.
import os
import numpy as np
from PIL import Image
from scipy import ndimage

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
src = os.path.join(ROOT, 'assets/brand/logo-source.png')
out = os.path.join(ROOT, 'assets/brand/logo.png')
a = np.asarray(Image.open(src).convert('RGB')).astype(np.float32)
h, w, _ = a.shape
bg = np.median(np.concatenate([a[:4, :w//4].reshape(-1,3), a[-4:].reshape(-1,3), a[:, :4].reshape(-1,3)]), axis=0)
d = np.sqrt(((a - bg) ** 2).sum(-1))
near = d < 40
lab, _ = ndimage.label(near)
edge = set(np.unique(np.concatenate([lab[0], lab[-1], lab[:, 0], lab[:, -1]]))) - {0}
back = np.isin(lab, list(edge))
# Soft edge: grow the shape by one pixel and ramp alpha by the distance to the background colour.
alpha = np.clip((d - 12) / 40, 0, 1)
alpha[~ndimage.binary_dilation(back, iterations=2)] = 1
alpha[back & (d < 12)] = 0
alpha = ndimage.gaussian_filter(alpha, 0.6)
# Undo the dark background in the edge pixels.
rgb = a.copy()
m = (alpha > 0.02) & (alpha < 0.98)
rgb[m] = np.clip((a[m] - bg * (1 - alpha[m, None])) / alpha[m, None], 0, 255)
# The gloss: a white highlight over the top of the circle, with a curved lower edge.
cx, cy, r = 620.0, 620.0, 572.0
yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
ex, ey, rx, ry = cx, cy - 0.62 * r, 1.25 * r, 0.98 * r
inside = ((xx - ex) / rx) ** 2 + ((yy - ey) / ry) ** 2
t = np.clip((yy - (cy - 1.15 * r)) / (ey + ry - (cy - 1.15 * r)), 0, 1)
gloss = np.where(inside <= 1, 0.30 * (1 - t) ** 1.6 + 0.05, 0)
edge_soft = np.clip((1 - inside) * 40, 0, 1)
circle = np.clip((r - np.sqrt((xx - cx) ** 2 + (yy - cy) ** 2)) / 6, 0, 1)
gloss = gloss * edge_soft * circle
rgb = rgb + (255 - rgb) * gloss[..., None]
outimg = np.dstack([np.clip(rgb, 0, 255), alpha * 255]).astype(np.uint8)
Image.fromarray(outimg, 'RGBA').save(out)

# The icons: the logo on the colour of the source background, with a small
# margin. UIPrerenderedIcon is true, so iOS adds no gloss of its own.
logo = Image.fromarray(outimg, 'RGBA')
for size, name in ((57, 'Icon.png'), (114, 'Icon@2x.png')):
    icon = Image.new('RGBA', (size, size), tuple(int(c) for c in bg) + (255,))
    inner = round(size * 0.94)
    icon.alpha_composite(logo.resize((inner, inner), Image.LANCZOS), ((size - inner) // 2, size - inner))
    icon.convert('RGB').save(os.path.join(ROOT, 'resources', name))
