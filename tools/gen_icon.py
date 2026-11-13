"""Draw the app icons (the Commodore "chicken head" C= logo) as BMP files for PilRC.

Palm OS 5 icon families: large 32x22 (and 64x44 for double density), small 15x9 (and 30x18), each in
1-bit and 8-bit colour.  The logo is drawn from its geometry (R = radius of the C, centre at 0,0, y down)
and rendered with supersampling (each pixel takes the colour most of its samples hit; no blending, so
the colours stay exact Palm palette colours).
Usage: python3 tools/gen_icon.py OUTDIR [--preview]   (--preview also writes a large preview.bmp)
"""
import struct, sys, os

BLUE = (0, 51, 153)      # in the Palm 8-bit palette
RED = (204, 0, 0)
WHITE = (255, 255, 255)
BLACK = (0, 0, 0)

# logo geometry, in units of the C's outer radius
C_INNER = 0.56          # inner radius of the C
C_CUT = 0.20            # the C's ends: everything right of x = C_CUT is removed
FLAG_LEFT = 0.27        # flags start just right of the C's ends
FLAG_RIGHT = 1.15       # outer (longest) edge of the flags
FLAG_TOP, FLAG_MID = 0.48, 0.06   # flags span |y| from FLAG_MID to FLAG_TOP
LOGO_LEFT, LOGO_RIGHT, LOGO_TOP, LOGO_BOTTOM = -1.0, FLAG_RIGHT, -1.0, 1.0


def logo_part(x, y, flag_gap=0.0):
    """Which part of the logo is at (x, y): 'C', 'T' (upper flag), 'B' (lower flag) or None.
    flag_gap moves the flags right (the large icon uses a wider gap between the C and the flags)."""
    if x > C_CUT:
        x -= flag_gap
        if x <= FLAG_LEFT:
            return None
    c = logo_colour(x, y)
    if c is None:
        return None
    if C_INNER * C_INNER <= x * x + y * y <= 1.0 and x <= C_CUT:
        return "C"
    return "T" if y < 0 else "B"


def logo_colour(x, y):
    """Colour of the logo at (x, y), or None for background."""
    r2 = x * x + y * y
    if C_INNER * C_INNER <= r2 <= 1.0 and x <= C_CUT:
        return BLUE
    ay = abs(y)
    if FLAG_MID <= ay <= FLAG_TOP and x >= FLAG_LEFT:
        # the inner (notch) corners are cut at 45 degrees: the right edge runs from FLAG_RIGHT at the
        # outside edge (|y| = FLAG_TOP) in to FLAG_RIGHT - (FLAG_TOP - FLAG_MID) at the notch
        right = FLAG_RIGHT - (FLAG_TOP - ay)
        if x <= right:
            return BLUE if y < 0 else RED
    return None


# --- the large icon, in the Palm OS 5 launcher style (studied from the TX's own icons, e.g. Calc,
# Calendar, Contacts):
#   * the object is turned slightly away: its front face is skewed so horizontal edges rise to the
#     right, and a little foreshortened;
#   * it is extruded: its thickness shows as darker faces on the left and bottom;
#   * a bold black outline round everything (1 px at 22x22, 2 px at 44x44);
#   * it stands on a big flat blue oval that fills the lower half, with a dithered gradient shadow
#     shadow falling back and up to the left (light from the front right).
# Size: the launcher's large icon is 32x22 (64x44 double density).  Measured in the TX simulator's
# launcher: the stock artwork is 56 x 44 px with a 56 x 22 px base filling the bottom half.
# Worked in whole pixels, so outlines and faces stay crisp.  k = 1 for 32x22, 2 for 64x44.
BASE = (102, 153, 204)
FRONT = {"C": (0, 51, 204), "T": (0, 51, 204), "B": (255, 0, 0)}   # lit faces
SIDE = {"C": (0, 0, 102), "T": (0, 0, 102), "B": (153, 0, 0)}      # extruded sides
SKEW = -0.18              # front face: y changes by this per unit of x (negative = rises to the right)
FORESHORTEN = 0.90        # front face: horizontal squash (turned away)
FRONT_BOX = (0.27, 0.07, 0.93, 0.78)   # the skewed front face is fitted into this box (fractions of the
                                       # icon's width and height); leave room for the black outline
BASE_ELLIPSE = (0.50, 0.75, 0.4375, 0.25)    # 56 x 22 px at 64x44: the bottom half, as measured
DEPTH = 2                 # extrusion in 32x22 pixels, towards the lower left (4 px at 64x44)
SHADOW_LENGTH = 10        # the shadow fades out over this many pixels left of the object (32x22 px)
SHADOW_WEIGHT = 0.7       # < 1 keeps it darker for longer before it thins out (as the stock ones do)
SHADOW_RISE = 0.5         # the shadow climbs this much per pixel leftwards (falls back behind the object)
# the stock icons' shadow: navy against the object, through mid blue, out to the base colour
SHADOW_RAMP = [BASE, (51, 102, 153), (0, 51, 153)]
BAYER4 = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]   # ordered-dither thresholds


def front_transform(w, h):
    """scale, ox, oy (pixels) so that pixel (u, v) = (ox + scale*FORESHORTEN*x, oy + scale*(y + SKEW*x))."""
    pts = [(LOGO_LEFT + (LOGO_RIGHT - LOGO_LEFT) * i / 40.0, LOGO_TOP + (LOGO_BOTTOM - LOGO_TOP) * j / 40.0)
           for i in range(41) for j in range(41)]
    pts = [(x, y) for (x, y) in pts if logo_part(x, y) is not None]
    us = [FORESHORTEN * x for x, y in pts]
    vs = [y + SKEW * x for x, y in pts]
    x0, y0, x1, y1 = FRONT_BOX[0] * w, FRONT_BOX[1] * h, FRONT_BOX[2] * w, FRONT_BOX[3] * h
    scale = min((x1 - x0) / (max(us) - min(us)), (y1 - y0) / (max(vs) - min(vs)))
    ox = x0 + ((x1 - x0) - (max(us) - min(us)) * scale) / 2 - min(us) * scale
    oy = y1 - max(vs) * scale                      # bottom-aligned, standing on the base
    return scale, ox, oy


def front_map(w, h, ss=8):
    """Which logo part covers each pixel of the w x h icon's (skewed) front face, or None."""
    scale, ox, oy = front_transform(w, h)
    m = [[None] * w for _ in range(h)]
    for py in range(h):
        for px in range(w):
            counts = {}
            for sy in range(ss):
                for sx in range(ss):
                    u = px + (sx + 0.5) / ss
                    v = py + (sy + 0.5) / ss
                    x = (u - ox) / (scale * FORESHORTEN)
                    y = (v - oy) / scale - SKEW * x
                    c = logo_part(x, y)
                    counts[c] = counts.get(c, 0) + 1
            m[py][px] = max(counts, key=counts.get)
    return m


def render_scene(w, h, mono):
    k = w // 32
    front = front_map(w, h)
    inside = lambda x, y: 0 <= x < w and 0 <= y < h
    # extruded sides: pixels behind the front face, towards the lower left
    side = [[None] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            if front[y][x] is None:
                for t in range(1, DEPTH * k + 1):
                    if inside(x + t, y - t) and front[y - t][x + t] is not None:
                        side[y][x] = front[y - t][x + t]
                        break
    solid = [[front[y][x] is not None or side[y][x] is not None for x in range(w)] for y in range(h)]
    # black outline: k pixels round the solid shape
    outline = [[False] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            if not solid[y][x]:
                outline[y][x] = any(inside(x + dx, y + dy) and solid[y + dy][x + dx]
                                    for dy in range(-k, k + 1) for dx in range(-k, k + 1))
    drawn = [[solid[y][x] or outline[y][x] for x in range(w)] for y in range(h)]
    cx, cy, rx, ry = BASE_ELLIPSE
    rows = []
    for y in range(h):
        row = []
        for x in range(w):
            if front[y][x] is not None:
                c = WHITE if mono else FRONT[front[y][x]]
            elif side[y][x] is not None:
                c = BLACK if mono else SIDE[side[y][x]]
            elif outline[y][x]:
                c = BLACK
            else:
                u, v = (x + 0.5) / w, (y + 0.5) / h
                if ((u - cx) / rx) ** 2 + ((v - cy) / ry) ** 2 <= 1.0:
                    # the shadow: falls back and up to the left, behind the object (a wedge, like the
                    # stock icons): strongest where it leaves the object, fading out with the distance
                    # measured down and to the right towards the object; ordered-dithered through the
                    # stock blues
                    t = 0.0
                    for d in range(1, SHADOW_LENGTH * k + 1):
                        ty = y + int(round(d * SHADOW_RISE))
                        if inside(x + d, ty) and drawn[ty][x + d]:
                            t = 1.0 - (d - 1) / (SHADOW_LENGTH * k)
                            break
                    level = (t ** SHADOW_WEIGHT) * (len(SHADOW_RAMP) - 1)   # 0 = base ... 2 = darkest
                    lo = int(level)
                    frac = level - lo
                    threshold = (BAYER4[y % 4][x % 4] + 0.5) / 16.0
                    step = lo + 1 if frac > threshold and lo + 1 < len(SHADOW_RAMP) else lo
                    if mono:
                        c = BLACK if (step >= 1 and threshold < t) or (x + y) % 4 == 0 else WHITE
                    else:
                        c = SHADOW_RAMP[step]
                else:
                    c = WHITE
            row.append(c)
        rows.append(row)
    return rows


def render(w, h, mono, margin=1, ss=8):
    """Render the logo centred in w x h pixels, as rows of RGB tuples."""
    lw, lh = LOGO_RIGHT - LOGO_LEFT, LOGO_BOTTOM - LOGO_TOP
    scale = min((w - 2 * margin) / lw, (h - 2 * margin) / lh)
    ox = (w - lw * scale) / 2 - LOGO_LEFT * scale
    oy = (h - lh * scale) / 2 - LOGO_TOP * scale
    rows = []
    for py in range(h):
        row = []
        for px in range(w):
            counts = {}
            for sy in range(ss):
                for sx in range(ss):
                    x = (px + (sx + 0.5) / ss - ox) / scale
                    y = (py + (sy + 0.5) / ss - oy) / scale
                    c = logo_colour(x, y)
                    counts[c] = counts.get(c, 0) + 1
            c = max(counts, key=counts.get)
            if c is None:
                c = WHITE
            elif mono:
                c = BLACK
            row.append(c)
        rows.append(row)
    return rows


def write_bmp(path, rows):
    h, w = len(rows), len(rows[0])
    pad = (4 - (w * 3) % 4) % 4
    data = b"".join(b"".join(struct.pack("BBB", b, g, r) for (r, g, b) in row) + b"\0" * pad
                    for row in reversed(rows))
    hdr = struct.pack("<2sIHHI", b"BM", 54 + len(data), 0, 0, 54)
    info = struct.pack("<IiiHHIIiiII", 40, w, h, 1, 24, 0, len(data), 2835, 2835, 0, 0)
    with open(path, "wb") as f:
        f.write(hdr + info + data)


def main():
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    # large icons: the logo on its base; small (list view) icons: the plain logo, as Palm's own are
    for name, (w, h) in {"lg": (32, 22), "lg2": (64, 44)}.items():
        write_bmp(os.path.join(out, "icon_%s_1.bmp" % name), render_scene(w, h, True))
        write_bmp(os.path.join(out, "icon_%s_8.bmp" % name), render_scene(w, h, False))
    for name, (w, h) in {"sm": (15, 9), "sm2": (30, 18)}.items():
        write_bmp(os.path.join(out, "icon_%s_1.bmp" % name), render(w, h, True, 0))
        write_bmp(os.path.join(out, "icon_%s_8.bmp" % name), render(w, h, False, 0))
    if "--preview" in sys.argv:
        big = render_scene(64, 44, False)                   # the 64x44 icon at 4x, for viewing
        write_bmp(os.path.join(out, "preview.bmp"), [[c for c in row for _ in range(4)] for row in big for _ in range(4)])


if __name__ == "__main__":
    main()
