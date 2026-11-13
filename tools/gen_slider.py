"""Draw the Settings box's volume slider (its track with a scale, and its thumb) as BMP files for PilRC.

The scale matches the slider's range, 0-200% (values 0-40 in steps of 5%): long marks at 0, 100 and
200%, short ones every 25%.  Palm OS puts the thumb's left edge at
(value - min) * (width - thumb width) / (max - min), so with a 72 x 15 track and an 8 x 15 thumb the
thumb's middle travels 64 standard pixels, from x = 4 to x = 68: a mark every 8.  Single and double
density, 8-bit (Palm palette colours only, so nothing is dithered).  The track is opaque (the form's
white); the thumb's corners are transparent (magenta).
Usage: python3 tools/gen_slider.py OUTDIR [--preview]
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen_icon import write_bmp  # noqa: E402

W, H, THUMB = 72, 15, 8           # standard pixels
MARKS = 9                         # 0, 25, ... 200%
WHITE, BLACK, KEY = (255, 255, 255), (0, 0, 0), (255, 0, 255)
G1, G2, G3, G4 = (51, 51, 51), (102, 102, 102), (153, 153, 153), (204, 204, 204)


def track(d):
    """The track, density d (1 or 2): a groove the thumb's middle runs along, the marks under it."""
    w, h = W * d, H * d
    rows = [[WHITE] * w for _ in range(h)]

    def box(x0, y0, x1, y1, c):
        for y in range(y0, y1):
            for x in range(x0, x1):
                rows[y][x] = c
    lo, hi = (THUMB // 2) * d, (W - THUMB // 2) * d       # the thumb's middle, first and last
    box(lo - d, 3 * d, hi + d, 6 * d, G2)                  # the groove: a dark rim, light inside
    box(lo, 4 * d, hi, 5 * d, G4)
    step = (hi - lo) // (MARKS - 1)
    for k in range(MARKS):
        x = lo + k * step - (d // 2)
        big = k % 4 == 0
        box(x, 8 * d, x + d, (14 if big else 11) * d, BLACK if big else G2)
    return rows


def thumb(d):
    """The thumb: a grey block with a point under it, aimed at the scale."""
    w, h = THUMB * d, H * d
    rows = [[KEY] * w for _ in range(h)]
    body_top, body_bot, tip = 1 * d, 7 * d, 10 * d
    for y in range(h):
        for x in range(w):
            u = (x + 0.5) / d - THUMB / 2                  # from the middle, standard pixels
            v = (y + 0.5) / d
            if body_top / d <= v < body_bot / d:
                inside = abs(u) < THUMB / 2 - 0.5
            elif body_bot / d <= v < tip / d:
                inside = abs(u) < (tip / d - v) * (THUMB / 2 - 0.5) / ((tip - body_bot) / d)
            else:
                inside = False
            if inside:
                rows[y][x] = G3
    for y in range(h):                                     # a dark outline, a light edge top left
        for x in range(w):
            if rows[y][x] != G3:
                continue
            nb = [rows[yy][xx] if 0 <= yy < h and 0 <= xx < w else KEY
                  for yy, xx in ((y - 1, x), (y + 1, x), (y, x - 1), (y, x + 1))]
            if KEY in nb:
                rows[y][x] = G1
    for y in range(h):
        for x in range(w):
            if rows[y][x] == G3 and (rows[y - 1][x] == G1 or rows[y][x - 1] == G1) and y < body_bot:
                rows[y][x] = G4
    return rows


def main():
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    for d, sfx in ((1, ""), (2, "2")):
        write_bmp(os.path.join(out, "slider_bg%s.bmp" % sfx), track(d))
        write_bmp(os.path.join(out, "slider_th%s.bmp" % sfx), thumb(d))
    if "--preview" in sys.argv:                            # the track with the thumb at 0, 100, 175%, 4x
        bg, th = track(2), thumb(2)
        for v in (0, 20, 35):
            x0 = v * (W - THUMB) * 2 // 40
            for y in range(H * 2):
                for x in range(THUMB * 2):
                    if th[y][x] != KEY:
                        bg[y][x0 + x] = th[y][x]
        write_bmp(os.path.join(out, "slider_preview.bmp"),
                  [[c for c in row for _ in range(4)] for row in bg for _ in range(4)])


if __name__ == "__main__":
    main()
