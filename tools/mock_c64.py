"""C64-style GUI mockups for the Palm TX (320x480), drawn pixel for pixel.

Writes design/mockups/<name>.png at 1:1 and <name>_2x.png for viewing, plus sheet_2x.png.
Usage: python tools/mock_c64.py
The 8x8 font below is our own, drawn in the C64's style (2-pixel vertical strokes); it is not a copy
of Commodore's character ROM.
"""
import os, struct, zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "design", "mockups")

# C64 colours (the common "Pepto" measurements); the TX can show them exactly in 16-bit colour mode
C64 = {
    "black": (0, 0, 0), "white": (255, 255, 255), "red": (104, 55, 43), "cyan": (112, 164, 178),
    "purple": (111, 61, 134), "green": (88, 141, 67), "blue": (53, 40, 121), "yellow": (184, 199, 111),
    "orange": (111, 79, 37), "brown": (67, 57, 0), "lightred": (154, 103, 89), "darkgrey": (68, 68, 68),
    "grey": (108, 108, 108), "lightgreen": (154, 210, 132), "lightblue": (108, 94, 181),
    "lightgrey": (149, 149, 149),
}

# --- font: rows top to bottom, '#' = ink.  Glyphs up to 7 wide, drawn from column 1 (6 wide) or
# column 0 (7 wide); row 7 is normally blank.
G = {
    "A": ["..##..", ".####.", "##..##", "######", "##..##", "##..##", "##..##"],
    "B": ["#####.", "##..##", "##..##", "#####.", "##..##", "##..##", "#####."],
    "C": [".####.", "##..##", "##....", "##....", "##....", "##..##", ".####."],
    "D": ["####..", "##.##.", "##..##", "##..##", "##..##", "##.##.", "####.."],
    "E": ["######", "##....", "##....", "#####.", "##....", "##....", "######"],
    "F": ["######", "##....", "##....", "#####.", "##....", "##....", "##...."],
    "G": [".####.", "##..##", "##....", "##.###", "##..##", "##..##", ".####."],
    "H": ["##..##", "##..##", "##..##", "######", "##..##", "##..##", "##..##"],
    "I": [".####.", "..##..", "..##..", "..##..", "..##..", "..##..", ".####."],
    "J": ["..####", "...##.", "...##.", "...##.", "...##.", "##.##.", ".###.."],
    "K": ["##..##", "##.##.", "####..", "###...", "####..", "##.##.", "##..##"],
    "L": ["##....", "##....", "##....", "##....", "##....", "##....", "######"],
    "M": ["##...##", "###.###", "#######", "##.#.##", "##...##", "##...##", "##...##"],
    "N": ["##..##", "###.##", "######", "######", "##.###", "##..##", "##..##"],
    "O": [".####.", "##..##", "##..##", "##..##", "##..##", "##..##", ".####."],
    "P": ["#####.", "##..##", "##..##", "#####.", "##....", "##....", "##...."],
    "Q": [".####.", "##..##", "##..##", "##..##", "##..##", ".####.", "...###"],
    "R": ["#####.", "##..##", "##..##", "#####.", "####..", "##.##.", "##..##"],
    "S": [".####.", "##..##", "##....", ".####.", "....##", "##..##", ".####."],
    "T": ["######", "..##..", "..##..", "..##..", "..##..", "..##..", "..##.."],
    "U": ["##..##", "##..##", "##..##", "##..##", "##..##", "##..##", ".####."],
    "V": ["##..##", "##..##", "##..##", "##..##", "##..##", ".####.", "..##.."],
    "W": ["##...##", "##...##", "##...##", "##.#.##", "#######", "###.###", "##...##"],
    "X": ["##..##", "##..##", ".####.", "..##..", ".####.", "##..##", "##..##"],
    "Y": ["##..##", "##..##", "##..##", ".####.", "..##..", "..##..", "..##.."],
    "Z": ["######", "....##", "...##.", "..##..", ".##...", "##....", "######"],
    "0": [".####.", "##..##", "##.###", "######", "###.##", "##..##", ".####."],
    "1": ["..##..", ".###..", "..##..", "..##..", "..##..", "..##..", "######"],
    "2": [".####.", "##..##", "....##", "...##.", "..##..", ".##...", "######"],
    "3": [".####.", "##..##", "....##", "..###.", "....##", "##..##", ".####."],
    "4": ["....##", "...###", "..####", ".##.##", "######", "....##", "....##"],
    "5": ["######", "##....", "#####.", "....##", "....##", "##..##", ".####."],
    "6": [".####.", "##..##", "##....", "#####.", "##..##", "##..##", ".####."],
    "7": ["######", "##..##", "...##.", "..##..", "..##..", "..##..", "..##.."],
    "8": [".####.", "##..##", "##..##", ".####.", "##..##", "##..##", ".####."],
    "9": [".####.", "##..##", "##..##", ".#####", "....##", "##..##", ".####."],
    " ": ["......"] * 7,
    ".": ["......"] * 5 + ["..##..", "..##.."],
    ",": ["......"] * 5 + ["..##..", "..##..", ".##..."],
    ":": ["......", "..##..", "..##..", "......", "..##..", "..##..", "......"],
    ";": ["......", "..##..", "..##..", "......", "..##..", "..##..", ".##..."],
    "-": ["......", "......", "......", "######", "......", "......", "......"],
    "\"": ["##..##", "##..##", "##..##", "......", "......", "......", "......"],
    "'": ["...##.", "..##..", "......", "......", "......", "......", "......"],
    "$": ["..##..", ".#####", "##....", ".####.", "....##", "#####.", "..##.."],
    "*": ["......", "##..##", ".####.", "######", ".####.", "##..##", "......"],
    "/": ["......", "....##", "...##.", "..##..", ".##...", "##....", "......"],
    "(": ["...##.", "..##..", ".##...", ".##...", ".##...", "..##..", "...##."],
    ")": [".##...", "..##..", "...##.", "...##.", "...##.", "..##..", ".##..."],
    "!": ["..##..", "..##..", "..##..", "..##..", "......", "......", "..##.."],
    "<": ["...##.", "..##..", ".##...", "##....", ".##...", "..##..", "...##."],
    ">": [".##...", "..##..", "...##.", "....##", "...##.", "..##..", ".##..."],
    "=": ["......", "......", "######", "......", "######", "......", "......"],
    "?": [".####.", "##..##", "....##", "...##.", "..##..", "......", "..##.."],
    "+": ["......", "..##..", "..##..", "######", "..##..", "..##..", "......"],
    "#": [".##.##", ".##.##", "#######", ".##.##", "#######", ".##.##", ".##.##"],
    "&": [".###..", "##.##.", ".###..", "###..#", "##.##.", "##..##", ".###.#"],
}
# graphics characters (full 8x8 cells)
GFX = {
    "█": ["########"] * 8,                                       # full block
    "▌": ["####...."] * 8,                                       # left half block
    "─": ["........"] * 3 + ["########"] * 2 + ["........"] * 3,   # box: horizontal
    "│": ["...##..."] * 8,                                       # box: vertical
    "┌": ["........"] * 3 + ["...#####"] * 2 + ["...##..."] * 3,   # corners
    "┐": ["........"] * 3 + ["#####..."] * 2 + ["...##..."] * 3,
    "└": ["...##..."] * 3 + ["...#####"] * 2 + ["........"] * 3,
    "┘": ["...##..."] * 3 + ["#####..."] * 2 + ["........"] * 3,
    "├": ["...##..."] * 3 + ["...#####"] * 2 + ["...##..."] * 3,
    "┤": ["...##..."] * 3 + ["#####..."] * 2 + ["...##..."] * 3,
    "▶": ["........", ".##.....", ".####...", ".######.", ".######.", ".####...", ".##.....", "........"],
    "■": ["........", ".######.", ".######.", ".######.", ".######.", ".######.", ".######.", "........"],
    "░": ["#.#.#.#.", ".#.#.#.#"] * 4,                          # dithered (dim) block
    "‖": ["........", ".##..##.", ".##..##.", ".##..##.", ".##..##.", ".##..##.", ".##..##.", "........"],  # pause
    "▲": ["........", "...##...", "..####..", ".######.", "########", "........", "........", "........"],
    "▼": ["........", "........", "########", ".######.", "..####..", "...##...", "........", "........"],
}


def glyph(ch):
    if ch in GFX:
        return [[c == "#" for c in r] for r in GFX[ch]]
    rows = G.get(ch.upper(), G["?"])
    off = 0 if len(rows[0]) == 7 else 1
    cell = [[False] * 8 for _ in range(8)]
    for y, r in enumerate(rows):
        for x, c in enumerate(r):
            if c == "#":
                cell[y][x + off] = True
    return cell


class Screen:
    """A 320x480 TX screen.  The bottom 30 px are the Palm status bar (input area closed)."""
    W, H, BAR = 320, 480, 30

    def __init__(self, bg):
        self.px = [[bg] * self.W for _ in range(self.H)]

    def rect(self, x, y, w, h, col):
        for yy in range(max(0, y), min(self.H, y + h)):
            for xx in range(max(0, x), min(self.W, x + w)):
                self.px[yy][xx] = col

    def text(self, x, y, s, fg, bg=None, scale=1, reverse=False):
        for i, ch in enumerate(s):
            g = glyph(ch)
            cx = x + i * 8 * scale
            for gy in range(8):
                for gx in range(8):
                    on = g[gy][gx] != reverse
                    col = fg if on else bg
                    if col is not None:
                        self.rect(cx + gx * scale, y + gy * scale, scale, scale, col)

    def status_bar(self):
        """A plain stand-in for the TX's status bar (home, menu, clock, input area toggle)."""
        y = self.H - self.BAR
        self.rect(0, y, self.W, self.BAR, (90, 90, 90))
        self.rect(0, y, self.W, 1, (40, 40, 40))
        for i, x in enumerate((8, 44, 110)):
            self.rect(x, y + 7, 28 if i < 2 else 56, 16, (150, 150, 150))
        for x in (238, 270, 288):
            self.rect(x, y + 7, 26, 16, (150, 150, 150))

    def save(self, name):
        os.makedirs(OUT, exist_ok=True)
        write_png(os.path.join(OUT, name + ".png"), self.px)
        write_png(os.path.join(OUT, name + "_2x.png"), [[c for c in r for _ in (0, 1)] for r in self.px for _ in (0, 1)])


def write_png(path, rows):
    h, w = len(rows), len(rows[0])
    raw = b"".join(b"\0" + b"".join(struct.pack("BBB", *c) for c in r) for r in rows)
    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


# sample content: the test tunes (block counts are file size / 254, as on a 1541)
TUNES = [("BACK TO THE FUTURE II", 21), ("BACK TO THE FUTURE III", 34), ("BLADE RUNNER", 9),
         ("CHEVY CHASE", 12), ("ELITE", 11), ("ELVIRA - THE ARCADE GAME", 36),
         ("FREESTYLER", 25), ("GHOSTBUSTERS", 14), ("KANE", 7),
         ("RAINBOW ISLANDS", 39), ("SUPREMACY", 29)]
SELECTED = 5
NOW = ("ELVIRA - THE ARCADE GAME", "ADAM GILMORE", "1991 FLAIR SOFTWARE", 1, 7, "01:23")


def mock_basic40():
    """1: the C64 BASIC screen, 40 columns of 8x8 text (38 inside a one-character border)."""
    lb, bl = C64["lightblue"], C64["blue"]
    s = Screen(lb)
    s.rect(8, 8, 304, s.H - s.BAR - 16, bl)
    x, y = 8, 8
    lines = [
        ("**** SID PLAYER V0.1 FOR PALM TX ****", False), ("", False),
        (" 11 TUNES ON CARD   591K BYTES FREE", False), ("", False),
        ("READY.", False), ('LOAD"$",8', False), ("", False),
        ("SEARCHING FOR $", False), ("LOADING", False), ("READY.", False), ("LIST", False), ("", False),
        ('0 "SID TUNES       " SD 2A', True),
    ]
    for name, blocks in TUNES:
        lines.append(('%-4d "%s" SID' % (blocks, name[:22]), len(lines) - 13 == SELECTED))
    lines += [("1294 BLOCKS FREE.", False), ("READY.", False), ("RUN", False), ("", False),
              ("NAME:     " + NOW[0][:28], False), ("AUTHOR:   " + NOW[1], False),
              ("RELEASED: " + NOW[2], False), ("", False),
              ("SONG %d/%d                TIME %s" % (NOW[3], NOW[4], NOW[5]), False), ("", False)]
    for i, (t, rev) in enumerate(lines):
        if rev:
            s.text(x, y + i * 8, t.ljust(38), lb, bl, reverse=True)
        else:
            s.text(x, y + i * 8, t, lb)
    yy = y + len(lines) * 8
    for v, n in ((1, 22), (2, 15), (3, 28)):          # voice levels
        s.text(x, yy, "V%d " % v, lb)
        s.text(x + 24, yy, "█" * n + "░" * (32 - n), lb)
        yy += 8
    yy += 8
    for i, b in enumerate((" << ", " STOP ", " >> ", " LIST ")):
        s.text(x + (0, 48, 112, 160)[i], yy, b, lb, bl, reverse=True)
    s.text(x, yy + 16, "█", lb)              # the cursor
    s.status_bar()
    s.save("1_basic40")


def mock_basic20():
    """2: the same, with the font doubled (16x16): 20 columns, about Palm's normal text size."""
    lb, bl = C64["lightblue"], C64["blue"]
    s = Screen(lb)
    s.rect(0, 16, 320, s.H - s.BAR - 32, bl)
    x, y = 0, 16
    def line(row, t, rev=False):
        s.text(x, y + row * 16, (t.ljust(20) if rev else t), lb, bl if rev else None, scale=2, reverse=rev)
    line(0, " *** SID PLAYER ***")
    line(1, "READY.")
    line(2, 'LOAD"$",8')
    for i, (name, blocks) in enumerate(TUNES[2:13]):
        line(3 + i, '"%s"' % name[:16].ljust(16) + (" " if True else ""), rev=(i + 2 == SELECTED))
    line(15, "READY.")
    line(16, "RUN")
    line(17, NOW[0][:20])
    line(18, NOW[1][:20])
    line(19, NOW[2][:20])
    line(20, "SONG %d/%d   %s" % (NOW[3], NOW[4], NOW[5]))
    yy = y + 21 * 16 + 8
    s.text(x, yy, " << ", lb, bl, scale=2, reverse=True)
    s.text(x + 72, yy, " STOP ", lb, bl, scale=2, reverse=True)
    s.text(x + 176, yy, " >> ", lb, bl, scale=2, reverse=True)
    s.text(x + 248, yy, "█", lb, scale=2)
    s.status_bar()
    s.save("2_basic20")


def mock_panels():
    """3: PETSCII panels: black screen, framed boxes in C64 colours, a level bar per voice."""
    s = Screen(C64["black"])
    def box(x, y, cols, rows, col, title=None):
        s.text(x, y, "┌" + "─" * (cols - 2) + "┐", col)
        for r in range(1, rows - 1):
            s.text(x, y + r * 12, "│", col)
            s.text(x + (cols - 1) * 8, y + r * 12, "│", col)
            s.rect(x + 3, y + r * 12 - 4, 2, 4, col)                      # join the 12-px rows
            s.rect(x + (cols - 1) * 8 + 3, y + r * 12 - 4, 2, 4, col)
        s.text(x, y + (rows - 1) * 12, "└" + "─" * (cols - 2) + "┘", col)
        s.rect(x + 3, y + (rows - 1) * 12 - 4, 2, 4, col)
        s.rect(x + (cols - 1) * 8 + 3, y + (rows - 1) * 12 - 4, 2, 4, col)
        if title:
            s.text(x + 16, y, " " + title + " ", C64["white"], C64["black"])
    s.text(0, 0, "  SID PLAYER V0.1        11 TUNES ".ljust(40), C64["lightred"], C64["black"], reverse=True)
    # now playing (rows 12 px apart: 8 px glyphs plus 4 px leading, easier to tap)
    box(0, 16, 40, 9, C64["yellow"], "NOW PLAYING")
    s.text(16, 28, NOW[0][:36], C64["yellow"])
    s.text(16, 40, NOW[1], C64["white"])
    s.text(16, 52, NOW[2], C64["lightgrey"])
    s.text(16, 70, "SONG", C64["lightgrey"])
    s.text(56, 70, "%d/%d" % (NOW[3], NOW[4]), C64["white"])
    s.text(240, 70, NOW[5], C64["white"])
    for i, (b, col) in enumerate(((" ▶ ", "lightgreen"), (" << ", "cyan"), (" >> ", "cyan"))):
        s.text(16 + i * 48, 88, b, C64[col], C64["black"], reverse=True)
    # voices
    box(0, 128, 40, 6, C64["cyan"], "VOICES")
    for v, (n, col) in enumerate(((22, "lightgreen"), (15, "yellow"), (28, "lightred"))):
        y = 140 + v * 12
        s.text(16, y, "%d" % (v + 1), C64["white"])
        s.text(32, y, "█" * n, C64[col])
        s.text(32 + n * 8, y, "░" * (34 - n), C64["darkgrey"])
    # tune list
    top = 208
    rows = (s.H - s.BAR - top) // 12
    box(0, top, 40, rows, C64["lightblue"], "TUNES ON CARD")
    for i, (name, blocks) in enumerate(TUNES[:rows - 2]):
        y = top + 12 + i * 12
        if i == SELECTED:
            s.text(16, y, name[:34].ljust(36), C64["blue"], C64["lightblue"], reverse=True)
        else:
            s.text(16, y, name[:34], C64["lightblue"])
    s.status_bar()
    s.save("3_panels")


def box12(s, x, y, cols, rows, col, title=None, title_fg=None, title_bg=None):
    """A box-drawing frame with rows 12 px apart (8 px glyphs plus 4 px leading)."""
    s.text(x, y, "┌" + "─" * (cols - 2) + "┐", col)
    for r in range(1, rows):
        for xx in (x, x + (cols - 1) * 8):
            s.rect(xx + 3, y + (r - 1) * 12 + 8, 2, 12, col)          # the sides, unbroken
    s.text(x, y + (rows - 1) * 12, "└" + "─" * (cols - 2) + "┘", col)
    if title:
        s.text(x + 16, y, " " + title + " ", title_fg, title_bg)


def mock_hybrid_panels():
    """4: option 3's panels and 12 px rows, in option 1's colours (blue screen, light blue border)."""
    lb, bl, wh = C64["lightblue"], C64["blue"], C64["white"]
    s = Screen(lb)
    s.rect(8, 8, 304, s.H - s.BAR - 16, bl)
    s.text(8, 8, " **** SID PLAYER V0.1 ****   11 TUNES ".ljust(38), lb, bl, reverse=True)
    box12(s, 8, 24, 38, 9, lb, "NOW PLAYING", wh, bl)
    s.text(24, 36, NOW[0][:34], wh)
    s.text(24, 48, NOW[1], lb)
    s.text(24, 60, NOW[2], lb)
    s.text(24, 78, "SONG %d/%d" % (NOW[3], NOW[4]), lb)
    s.text(232, 78, NOW[5], wh)
    for i, b in enumerate((" ▶ ", " << ", " >> ")):
        s.text(24 + i * 48, 96, b, lb, bl, reverse=True)
    box12(s, 8, 136, 38, 6, lb, "VOICES", wh, bl)
    for v, n in enumerate((22, 15, 28)):
        y = 148 + v * 12
        s.text(24, y, "%d" % (v + 1), wh)
        s.text(40, y, "█" * n, lb)
        s.text(40 + n * 8, y, "░" * (32 - n), lb)
    top = 216
    rows = (s.H - s.BAR - 8 - top) // 12
    box12(s, 8, top, 38, rows, lb, "TUNES ON CARD", wh, bl)
    for i, (name, blocks) in enumerate(TUNES[:rows - 2]):
        y = top + 12 + i * 12
        line = "%-3d %s" % (blocks, name[:30])
        if i == SELECTED:
            s.text(24, y, line.ljust(34), lb, bl, reverse=True)
        else:
            s.text(24, y, line, lb)
    s.status_bar()
    s.save("4_hybrid_panels")


def mock_hybrid_basic():
    """5: option 1's BASIC screen (READY./LOAD/LIST/RUN), with option 3's 12 px rows."""
    lb, bl = C64["lightblue"], C64["blue"]
    s = Screen(lb)
    s.rect(8, 8, 304, s.H - s.BAR - 16, bl)
    lines = [("**** SID PLAYER V0.1 FOR PALM TX ****", False),
             (" 11 TUNES ON CARD   591K BYTES FREE", False),
             ("READY.", False), ('LOAD"$",8', False),
             ('0 "SID TUNES       " SD 2A', True)]
    for i, (name, blocks) in enumerate(TUNES):
        lines.append(('%-4d "%s" SID' % (blocks, name[:22]), i == SELECTED))
    lines += [("1294 BLOCKS FREE.", False), ("READY.", False), ("RUN", False),
              ("NAME:     " + NOW[0][:28], False), ("AUTHOR:   " + NOW[1], False),
              ("RELEASED: " + NOW[2], False),
              ("SONG %d/%d                TIME %s" % (NOW[3], NOW[4], NOW[5]), False)]
    y = 12
    for t, rev in lines:
        if rev:
            s.text(8, y, t.ljust(38), lb, bl, reverse=True)
        else:
            s.text(8, y, t, lb)
        y += 12
    for v, n in ((1, 22), (2, 15), (3, 28)):
        s.text(8, y, "V%d " % v, lb)
        s.text(32, y, "█" * n + "░" * (32 - n), lb)
        y += 12
    y += 4
    for i, b in enumerate((" << ", " STOP ", " >> ", " LIST ")):
        s.text(8 + (0, 48, 112, 160)[i], y, b, lb, bl, reverse=True)
    s.text(8, y + 16, "█", lb)
    s.status_bar()
    s.save("5_hybrid_basic")


def sheet():
    imgs = []
    for n in ("1_basic40", "2_basic20", "3_panels"):
        with open(os.path.join(OUT, n + "_2x.png"), "rb") as f:
            imgs.append(n)
    # simple side-by-side sheet at 1:1 (each 320x480, 16 px gaps), re-rendered from the screens
    return imgs


if __name__ == "__main__":
    mock_basic40()
    mock_basic20()
    mock_panels()
    mock_hybrid_panels()
    mock_hybrid_basic()
    print("wrote", OUT)
