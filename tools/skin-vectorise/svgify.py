#!/usr/bin/env python3
"""Vectorise the SpectrumWorx skin's pill buttons.

Each button is measured off its PNG -- rounded-rect geometry, the vertical
gradient, the white outer glow and the blue rim -- and re-emitted as paths.
The label is set in Bitstream Vera Sans Bold, the skin's own font, and
converted to outlines so the SVG carries no font dependency. No filters: JUCE
8's SVG parser has none, so the glow is concentric paths.
"""
import os
import subprocess
import numpy as np
from PIL import Image
import os
_HERE = os.path.dirname(os.path.abspath(__file__))
_ROOT = os.path.abspath(os.path.join(_HERE, "..", ".."))
SKIN = os.path.join(_ROOT, "assets", "skin")
TMP = os.environ.get("SKIN_VECTORISE_TMP", os.path.join(_HERE, ".work"))
os.makedirs(TMP, exist_ok=True)

from fontTools.ttLib import TTFont
from fontTools.pens.recordingPen import RecordingPen
from fontTools.pens.boundsPen import BoundsPen



FONT = os.path.join(SKIN, "VeraBd.ttf")
TEXT_FILL = "#231f20"
RIM_FILL = "#12b4ea"
SS = 8

# (highlighted number, plain number, label)
PILLS = [(8, 9, "PRESETS"), (10, 11, "SETTINGS"),
         (30, 31, "Save"), (32, 33, "Delete"), (34, 35, "Save as")]


# ---------------------------------------------------------------- numbers --
def fmt(v, dp=2):
    s = "%.*f" % (dp, v + 0.0)
    if "." in s:
        s = s.rstrip("0").rstrip(".")
    if s in ("-0", ""):
        s = "0"
    if s.startswith("0.") and len(s) > 2:
        s = s[1:]
    elif s.startswith("-0.") and len(s) > 3:
        s = "-" + s[2:]
    return s


def join(tokens):
    out, prev = "", None
    for t in tokens:
        if t[0].isalpha():
            out += t
            prev = None
            continue
        if prev is None or t.startswith("-") or (t.startswith(".") and "." in prev):
            pass
        else:
            out += " "
        out += t
        prev = t
    return out


def load(n):
    return np.asarray(Image.open(os.path.join(SKIN, "%02d.png" % n)).convert("RGBA"),
                      dtype=np.float64) / 255.0


# ------------------------------------------------------------- geometry ----
def field(shape, box):
    """Signed distance to the rounded rect, supersampled."""
    H, W = shape
    x0, x1, y0, y1, r = box
    ys, xs = np.mgrid[0:H * SS, 0:W * SS]
    px, py = (xs + .5) / SS, (ys + .5) / SS
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    dx = np.abs(px - cx) - ((x1 - x0) / 2 - r)
    dy = np.abs(py - cy) - ((y1 - y0) / 2 - r)
    return (np.hypot(np.maximum(dx, 0), np.maximum(dy, 0))
            + np.minimum(np.maximum(dx, dy), 0) - r)


def down(a):
    H = a.shape[0] // SS
    W = a.shape[1] // SS
    if a.ndim == 2:
        return a.reshape(H, SS, W, SS).mean(axis=(1, 3))
    return a.reshape(H, SS, W, SS, a.shape[2]).mean(axis=(1, 3))


def descend(x0, cost, steps, rounds=60, floor=None):
    best = list(x0)
    b = cost(best)
    while True:
        moved = False
        for i in range(len(best)):
            for s in (+steps[i], -steps[i]):
                c = list(best)
                c[i] += s
                if floor and floor(i, c[i]) is False:
                    continue
                v = cost(c)
                if v < b:
                    best, b, moved = c, v, True
        if not moved:
            steps = [s / 2 for s in steps]
            if max(steps) < 1e-3:
                break
    return best, b


def fit_geometry(plain):
    """Rounded rect of the button body, from the plain (un-glowing) PNG alpha."""
    a = plain[:, :, 3]
    H, W = a.shape
    ys, xs = np.nonzero(a > 0.5)
    box0 = [xs.min() + .5, xs.max() + .5, ys.min() + .5, ys.max() + .5, 4.5]

    def cost(p):
        if p[4] < 0.5 or p[1] <= p[0] or p[3] <= p[2]:
            return 1e9
        if p[4] > min(p[1] - p[0], p[3] - p[2]) / 2:
            return 1e9
        return np.abs(down((field((H, W), p) <= 0).astype(float)) - a).mean()

    box, err = descend(box0, cost, [.4] * 5)
    return box, err


def fit_gradient(plain, box):
    """Vertical gradient of the body: the brightest interior pixel per row."""
    rgb = plain[:, :, :3]
    a = plain[:, :, 3]
    d = down(field(plain.shape[:2], box))
    rows, vals = [], []
    for y in range(plain.shape[0]):
        sel = (d[y] < -1.5) & (a[y] > .99)
        if sel.sum() < 3:
            continue
        rows.append(y + .5)
        vals.append(rgb[y][sel].max())
    if len(rows) < 3:
        return None
    A = np.polyfit(rows, vals, 1)                 # value = A[0]*y + A[1]
    x0, x1, y0, y1 = box[:4]
    top = A[0] * y0 + A[1]
    bot = A[0] * y1 + A[1]
    # a gradient that would exceed white gets a flat run at the top instead
    off = 0.0
    if top > 1.0:
        ywhite = (1.0 - A[1]) / A[0]
        off = (ywhite - y0) / (y1 - y0)
        top = 1.0
    return dict(offset=max(0.0, off), top=top, bottom=np.clip(bot, 0, 1))


def fit_highlight(hi, box, grad):
    """White glow rings and the blue rim, from the highlighted PNG."""
    H, W = hi.shape[:2]
    D = field((H, W), box)
    tgt_pm = hi[:, :, :3] * hi[:, :, 3:4]
    ys = (np.mgrid[0:H * SS, 0:W * SS][0] + .5) / SS
    x0, x1, y0, y1 = box[:4]
    gy0 = y0 + grad["offset"] * (y1 - y0)
    t = np.clip((ys - gy0) / max(y1 - gy0, 1e-6), 0, 1)
    body = grad["top"] + (grad["bottom"] - grad["top"]) * t
    body3 = np.repeat(body[:, :, None], 3, axis=2)
    mask = down((D > -2.2).astype(float)) > .5

    def render(p):
        o = p[:5]
        ain, aout = p[5], p[6]
        S = np.array(p[7:10])
        pm = np.zeros((H * SS, W * SS, 3))
        al = np.zeros((H * SS, W * SS))
        for e, op in zip((5, 4, 3, 2, 1), o):
            c = (D <= e).astype(float) * op
            pm = c[:, :, None] + pm * (1 - c)[:, :, None]
            al = c + al * (1 - c)
        c = (D <= 0).astype(float)
        pm = body3 * c[:, :, None] + pm * (1 - c)[:, :, None]
        al = c + al * (1 - c)
        c = ((D >= ain) & (D <= aout)).astype(float)
        pm = S[None, None, :] * c[:, :, None] + pm * (1 - c)[:, :, None]
        al = c + al * (1 - c)
        return down(pm), down(al)

    def cost(p):
        if not all(0 <= v <= .6 for v in p[:5]):
            return 1e9
        if p[5] > p[6] or not all(0 <= v <= 1 for v in p[7:10]):
            return 1e9
        pm, al = render(p)
        e = np.abs(pm - tgt_pm).sum(axis=2) + np.abs(al - hi[:, :, 3])
        return e[mask].mean()

    p0 = [.014, .010, .045, .069, .115, -.56, .25, .07, .72, .92]
    p, err = descend(p0, cost, [.02] * 5 + [.15, .15] + [.03] * 3)
    return dict(rings=list(zip((5, 4, 3, 2, 1), p[:5])),
                rim_in=p[5], rim_out=p[6],
                rim_rgb=tuple(int(round(v * 255)) for v in p[7:10])), err


# ----------------------------------------------------------------- text ----
_font = TTFont(FONT)
_upm = _font["head"].unitsPerEm
_cmap = _font.getBestCmap()
_hmtx = _font["hmtx"]
_gs = _font.getGlyphSet()
_bp = BoundsPen(_gs)
_gs[_cmap[ord("H")]].draw(_bp)
_CAP = _bp.bounds[3]


def glyph_run(word):
    recs, advs = [], []
    for ch in word:
        n = _cmap[ord(ch)]
        p = RecordingPen()
        _gs[n].draw(p)
        recs.append(p.value)
        advs.append(_hmtx[n][0])
    return recs, advs


def text_d(word, cap_px, tracking, baseline, centre_x):
    """Glyph outlines as one path, positioned by cap height / baseline / centre."""
    recs, advs = glyph_run(word)
    scale = cap_px / (_CAP / _upm) / _upm
    segs = []
    minx, maxx = 1e9, -1e9
    x = 0.0
    for rec, adv in zip(recs, advs):
        for op, args in rec:
            pts = [p for p in args if p is not None]
            tp = [(x + q[0] * scale, baseline - q[1] * scale) for q in pts]
            for q in tp:
                minx = min(minx, q[0]); maxx = max(maxx, q[0])
            segs.append((op, tp, len(args) > 0 and args[-1] is None))
        x += adv * scale + tracking
    dx = centre_x - (minx + maxx) / 2
    segs = [(op, [(a + dx, b) for a, b in pts], f) for op, pts, f in segs]

    toks, cur, start, last = [], None, None, None

    def emit(cmd, nums):
        nonlocal last
        if cmd != last:
            toks.append(cmd)
            last = cmd
        toks.extend(nums)

    for op, pts, implied in segs:
        if op == "closePath":
            toks.append("Z"); last = None; cur = start; continue
        if op == "moveTo":
            p = pts[0]; emit("M", [fmt(p[0]), fmt(p[1])]); last = "L"; cur = start = p; continue
        if op == "lineTo":
            p = pts[0]
            if fmt(p[1]) == fmt(cur[1]):
                emit("H", [fmt(p[0])])
            elif fmt(p[0]) == fmt(cur[0]):
                emit("V", [fmt(p[1])])
            else:
                emit("L", [fmt(p[0]), fmt(p[1])])
            cur = p
            continue
        if op == "qCurveTo":
            run = list(pts) + ([start] if implied else [])
            if len(run) == 1:
                run = [run[0], run[0]]
            for i in range(len(run) - 2):
                c, n = run[i], run[i + 1]
                emit("Q", [fmt(c[0]), fmt(c[1]),
                           fmt((c[0] + n[0]) / 2), fmt((c[1] + n[1]) / 2)])
            emit("Q", [fmt(run[-2][0]), fmt(run[-2][1]), fmt(run[-1][0]), fmt(run[-1][1])])
            cur = run[-1]
            continue
        if op == "curveTo":
            emit("C", [f for q in pts for f in (fmt(q[0]), fmt(q[1]))])
            cur = pts[-1]
    return join(toks), (minx + dx, maxx + dx)


def ink_mask(png, box, grad):
    """Per-pixel text coverage, plus the weight it can be trusted with.

    The body gradient runs down to near-black, and the label is near-black
    throughout, so towards the bottom of the button there is no contrast left
    to read ink from. Those rows are weighted out rather than guessed at --
    otherwise the dark tail of the gradient reads as one enormous glyph."""
    H, W = png.shape[:2]
    d = down(field((H, W), box))
    y = np.arange(H)[:, None] + .5
    x0, x1, y0, y1 = box[:4]
    gy0 = y0 + grad["offset"] * (y1 - y0)
    bg = grad["top"] + (grad["bottom"] - grad["top"]) * np.clip((y - gy0) / max(y1 - gy0, 1e-6), 0, 1)
    ink_c = 35 / 255.0
    contrast = bg - ink_c
    cov = np.clip((bg - png[:, :, 0]) / np.maximum(contrast, 1e-6), 0, 1)
    weight = np.broadcast_to((contrast > .18).astype(float), cov.shape).copy()
    weight[(d > -1.0) | (png[:, :, 3] < .99)] = 0     # keep clear of the rim
    return cov * weight, weight


def render_text_only(d, H, W, path):
    svg = ('<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d">'
           '<path d="%s" fill="#000"/></svg>' % (W, H, W, H, d))
    with open(path + ".svg", "w") as fh:
        fh.write(svg)
    subprocess.run(["rsvg-convert", "-w", str(W), "-h", str(H), path + ".svg", "-o", path + ".png"],
                   check=True)
    return np.asarray(Image.open(path + ".png").convert("RGBA"), dtype=np.float64)[:, :, 3] / 255.0


def fit_text(png, box, grad, word, tag):
    """Cap height and baseline are taken from the PNG so the label keeps its
    size and seating; the spacing stays Vera's natural tracking and the run is
    centred on the button rather than stretched to the old ink width."""
    H, W = png.shape[:2]
    target, weight = ink_mask(png, box, grad)
    ys, _ = np.nonzero(target > .35)
    centre = (box[0] + box[1]) / 2
    scratch = os.path.join(TMP, "t_" + tag)
    wsum = max(weight.sum(), 1.0)

    def cost(p):
        cap, base = p
        if cap < 2 or cap > 14:
            return 1e9
        d, _ = text_d(word, cap, 0.0, base, centre)
        got = render_text_only(d, H, W, scratch) * weight
        return float((np.abs(got - target)).sum() / wsum)

    p, err = descend([6.0, ys.max() + 1.0], cost, [.4, .5])
    return dict(cap=p[0], tracking=0.0, baseline=p[1], centre=centre), err


# ----------------------------------------------------------------- emit ----
def pill_path(box, expand=0.0):
    x0, x1, y0, y1, r = box
    x0 -= expand; y0 -= expand; x1 += expand; y1 += expand; r += expand
    f = fmt
    return ("M%s %sH%sA%s %s 0 0 1 %s %sV%sA%s %s 0 0 1 %s %sH%sA%s %s 0 0 1 %s %sV%sA%s %s 0 0 1 %s %sZ"
            % (f(x0 + r), f(y0), f(x1 - r), f(r), f(r), f(x1), f(y0 + r), f(y1 - r),
               f(r), f(r), f(x1 - r), f(y1), f(x0 + r), f(r), f(r), f(x0), f(y1 - r),
               f(y0 + r), f(r), f(r), f(x0 + r), f(y0)))


def hexc(rgb):
    return "#%02x%02x%02x" % rgb


def emit(W, H, box, grad, hl, text):
    L = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d">'
         % (W, H, W, H)]
    top = hexc((round(grad["top"] * 255),) * 3)
    bot = hexc((round(grad["bottom"] * 255),) * 3)
    if top == "#ffffff":
        top = "#fff"
    L.append('<defs><linearGradient id="g" gradientUnits="userSpaceOnUse" x1="0" y1="%s" x2="0" y2="%s">'
             '<stop offset="%s" stop-color="%s"/><stop offset="1" stop-color="%s"/>'
             '</linearGradient></defs>'
             % (fmt(box[2]), fmt(box[3]), fmt(grad["offset"], 3), top, bot))
    if hl:
        for e, o in hl["rings"]:
            if o < 0.002:
                continue
            L.append('<path d="%s" fill="#fff" fill-opacity="%s"/>' % (pill_path(box, e), fmt(o, 3)))
    L.append('<path d="%s" fill="url(#g)"/>' % pill_path(box))
    if hl:
        L.append('<path d="%s%s" fill="%s" fill-rule="evenodd"/>'
                 % (pill_path(box, hl["rim_out"]), pill_path(box, hl["rim_in"]), RIM_FILL))
    L.append('<path d="%s" fill="%s"/>' % (text, TEXT_FILL))
    L.append("</svg>")
    return "\n".join(L) + "\n"
