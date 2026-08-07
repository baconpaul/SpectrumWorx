#!/usr/bin/env python3
"""Vectorise 01.png, the SpectrumWorx editor background.

Same method as the other generators: every number here is measured off the PNG
rather than eyeballed, and the result is rendered back with rsvg-convert and
compared -- per region as well as overall, because a mean taken over the whole
563x376 canvas is flattered by how much of it is flat.

The construction found:

  Two rounded plates on an #e6e6e6 surround, each carrying a 45-degree linear
  gradient, and the two mirror each other: the main display's ramp runs along
  x-y, from #231f20 at its lower left up to cyan at its upper right; the left
  strip's runs along y-x, so its cyan lands bottom left.  Both are flat for
  their first stretch and then accelerate, so they are emitted as a fitted set
  of stops rather than as two.  Two flat tabs bridge the gap between the
  plates, drawn under both so their ends need no geometry.  Each plate carries
  a pale rim a pixel inside its edge -- about three times heavier across the
  top and bottom than down the sides -- and the main display a dark line on
  its outermost column.

  Inside the left strip a plate -- the module column -- holds six rounded
  panels, each a 1px white ring at a fitted width over a fill.  Where a panel
  reaches far enough down-left to meet the ramp its fill is that same ramp
  scaled: the panels are a dark wash over the plate rather than opaque, and one
  scale factor per panel reproduces them to about a level.  The selected slot
  is a #12b4ea ring with a white glow, as 56 is.  The LFO panel's top edge
  steps down over its left third -- two quarter arcs sharing a radius, with the
  label in the step.

  Three knobs, drawn over the column's own frame and under the panels, which is
  the order the artwork has: the frame's line is blacked out where a knob
  crosses it and the panels' lines are not.  Each is a black disc plus a bright
  arc over its lower left -- the artwork's ring is not a ring, it fades out
  across the top and is buried under the column on the right.

  Text is Bitstream Vera outlines, the skin's own font, with cap height,
  baseline and centre fitted against an ink mask.  The weight is chosen per
  group and by ink -- mass and stroke width, not L1, which at these sizes
  simply prefers whichever face lays down less.  It picks bold for the knob
  labels and the panel labels and regular for the wordmark.

Deliberately approximated: the logo swash is redrawn as two stroked paths
fitted to the artwork's stroke runs rather than traced outline by outline, and
the wordmark is set in Vera where the artwork used a lighter, narrower
humanist face -- the same face, and the same compromise, as the About page
noted in the README.  Vera cannot match both that face's cap height and its
width, and the fit takes the width, so the wordmark reads a little short.

Run:  python3 tools/skin-vectorise/run_background.py
It is idempotent -- re-running reproduces the committed file byte for byte.
"""
import os
import subprocess
import sys

import numpy as np
from PIL import Image
from fontTools.ttLib import TTFont
from fontTools.pens.boundsPen import BoundsPen

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import svgify                                                        # noqa: E402
from svgify import SKIN, TMP, SS, fmt, join, down, descend, hexc     # noqa: E402

N = 1
PNG = os.path.join(SKIN, "%02d.png" % N)
SVG = os.path.join(SKIN, "%02d.svg" % N)
BG = 230.0
BLUE = "#12b4ea"                      # the skin's cyan, as in svgify.RIM_FILL
LEFT, MAIN = -1, +1                   # which way a plate's ramp runs: y-x, x-y


def load():
    return np.asarray(Image.open(PNG).convert("RGB"), dtype=np.float64)


def _i(rgb):
    return tuple(int(round(float(v))) for v in np.clip(rgb, 0, 255))


# ----------------------------------------------------------------- fonts ---
# svgify's outline emitter is bound to VeraBd at import time; the wordmark is
# the regular weight, so the face is swapped in place rather than the emitter
# copied -- one code path lays out every glyph on the page.
def face(name):
    f = TTFont(os.path.join(SKIN, name))
    gs = f.getGlyphSet()
    cmap = f.getBestCmap()
    bp = BoundsPen(gs)
    gs[cmap[ord("H")]].draw(bp)               # Vera's OS/2 carries no sCapHeight
    return dict(font=f, upm=f["head"].unitsPerEm, cmap=cmap,
                hmtx=f["hmtx"], gs=gs, cap=bp.bounds[3])


FACES = {}


def text_d(weight, word, cap, baseline, centre):
    f = FACES[weight]
    svgify._font, svgify._upm = f["font"], f["upm"]
    svgify._cmap, svgify._hmtx, svgify._gs = f["cmap"], f["hmtx"], f["gs"]
    svgify._CAP = f["cap"]
    return svgify.text_d(word, cap, 0.0, baseline, centre)[0]


# -------------------------------------------------------------- geometry ---
class Win:
    """A supersampled sample grid over one rectangle of the canvas."""

    def __init__(self, x0, x1, y0, y1):
        self.box = (int(x0), int(x1), int(y0), int(y1))
        xs = np.arange(int(x0) * SS, int(x1) * SS)
        ys = np.arange(int(y0) * SS, int(y1) * SS)
        self.px = (xs[None, :] + .5) / SS
        self.py = (ys[:, None] + .5) / SS

    def crop(self, img):
        x0, x1, y0, y1 = self.box
        return img[y0:y1, x0:x1]


def rr_path(g, inset=0.0):
    """A rounded rect, optionally eroded by `inset`; a negative inset grows it."""
    x0, x1, y0, y1 = g[0] + inset, g[1] - inset, g[2] + inset, g[3] - inset
    r = list(g[4:])
    rtl, rtr, rbr, rbl = [max(float(v) - inset, 0.0) for v in (r * 4 if len(r) == 1 else r)]
    t = ["M", fmt(x0 + rtl), fmt(y0)]

    def arc(rad, x, y):
        if rad > 0:                    # a square corner needs no arc command
            t.extend(["A", fmt(rad), fmt(rad), "0", "0", "1", fmt(x), fmt(y)])

    t.extend(["H", fmt(x1 - rtr)]); arc(rtr, x1, y0 + rtr)
    t.extend(["V", fmt(y1 - rbr)]); arc(rbr, x1 - rbr, y1)
    t.extend(["H", fmt(x0 + rbl)]); arc(rbl, x0, y1 - rbl)
    t.extend(["V", fmt(y0 + rtl)]); arc(rtl, x0 + rtl, y0)
    t.append("Z")
    return join(t)


def notch_path(g, step, inset=0.0):
    """The LFO panel: a rounded rect whose top edge steps up over its right.

    `step` is (x of the step's left foot, x of its right shoulder, y of the
    high edge, corner radius).  The step is two quarter arcs meeting halfway,
    so eroding it is the two radii moving apart by the inset and nothing else:
    the concave arc's centre lies outside the panel and the convex one's
    inside."""
    xa, xb, yhi0, r = step
    x0, x1, y1 = g[0] + inset, g[1] - inset, g[3] - inset
    mid = ((g[2] + yhi0) / 2, (xa + xb) / 2 + inset)   # the two arcs' meeting point
    ylo, yhi = g[2] + inset, yhi0 + inset
    rtr, rbr, rbl = [max(float(v) - inset, 0.0) for v in (g[4], g[5], g[6])]
    rtl = max(float(g[7]) - inset, 0.0)
    f = fmt
    t = ["M", f(x0 + rtl), f(ylo), "H", f(xa),
         "A", f(r + inset), f(r + inset), "0", "0", "0", f(mid[1]), f(mid[0]),
         "A", f(r - inset), f(r - inset), "0", "0", "1", f(xb), f(yhi),
         "H", f(x1 - rtr),
         "A", f(rtr), f(rtr), "0", "0", "1", f(x1), f(yhi + rtr),
         "V", f(y1 - rbr),
         "A", f(rbr), f(rbr), "0", "0", "1", f(x1 - rbr), f(y1),
         "H", f(x0 + rbl),
         "A", f(rbl), f(rbl), "0", "0", "1", f(x0), f(y1 - rbl),
         "V", f(ylo + rtl),
         "A", f(rtl), f(rtl), "0", "0", "1", f(x0 + rtl), f(ylo), "Z"]
    return join(t)


def circ_path(cx, cy, r):
    f = fmt
    return join(["M", f(cx - r), f(cy), "A", f(r), f(r), "0", "0", "0", f(cx + r), f(cy),
                 "A", f(r), f(r), "0", "0", "0", f(cx - r), f(cy), "Z"])


def arc_path(cx, cy, r, a0, a1):
    """An open arc from a0 to a1 degrees, measured anticlockwise from east."""
    f = fmt
    p0 = (cx + r * np.cos(np.radians(a0)), cy - r * np.sin(np.radians(a0)))
    p1 = (cx + r * np.cos(np.radians(a1)), cy - r * np.sin(np.radians(a1)))
    large = "1" if abs(a1 - a0) > 180 else "0"
    return join(["M", f(p0[0]), f(p0[1]), "A", f(r), f(r), "0", large, "0",
                 f(p1[0]), f(p1[1])])


def sdf(px, py, g):
    """Signed distance to a rounded rect, one radius or four (tl, tr, br, bl)."""
    x0, x1, y0, y1 = g[:4]
    r = list(g[4:])
    rtl, rtr, rbr, rbl = r * 4 if len(r) == 1 else r
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    rad = np.where(py >= cy, np.where(px >= cx, rbr, rbl),
                   np.where(px >= cx, rtr, rtl))
    dx = np.abs(px - cx) - ((x1 - x0) / 2 - rad)
    dy = np.abs(py - cy) - ((y1 - y0) / 2 - rad)
    return (np.hypot(np.maximum(dx, 0), np.maximum(dy, 0))
            + np.minimum(np.maximum(dx, dy), 0) - rad)


# ------------------------------------------------------------ edge maths ---
def outer_edge(lum, axis, approx, along, first, base):
    """Where an opaque plate meets the #e6e6e6 surround.

    One pixel is partly covered and everything past it is plate, which is all
    a straight opaque edge needs.  The reference is the plate's own colour, not
    the neighbouring pixel: a rim two pixels in would otherwise read as ink."""
    out = []
    for t in along:
        lo = int(approx) - 3
        p = lum[t, lo:lo + 7] if axis == "h" else lum[lo:lo + 7, t]
        idx = np.nonzero(p < BG - 1)[0]
        if not len(idx):
            continue
        i = idx[0] if first else idx[-1]
        c = min((BG - p[i]) / max(BG - base, 1e-6), 1.0)
        out.append(lo + i + 1 - c if first else lo + i + c)
    return float(np.median(out)) if out else float("nan")


def plate_radius(lum, g, base, r0=9.0):
    """The corner radius, from where the edge lands row by row round a corner.

    Fitted off the same opaque-edge reading as the sides rather than off a
    coverage map, so the rim inside the edge cannot pull it in."""
    x0, x1, y0, y1 = g[:4]
    obs = []
    for dy in np.arange(1, r0 + 3):
        t = int(round(y0 + dy))
        obs.append((dy, outer_edge(lum, "h", x0, [t], True, base) - x0))
        t = int(round(y1 - dy))
        obs.append((dy, outer_edge(lum, "h", x0, [t], True, base) - x0))
    obs = [(d, v) for d, v in obs if np.isfinite(v) and -1 < v < r0 + 3]

    def cost(p):
        r = p[0]
        if r < 2 or r > 20:
            return 1e9
        e = 0.0
        for d, v in obs:
            dy = max(r - d, 0.0)
            e += (r - np.sqrt(max(r * r - dy * dy, 0.0)) - v) ** 2
        return e

    p, err = descend([r0], cost, [.4])
    return p[0], err / max(len(obs), 1)


def band_fit(prof, p0, a0, w0, ink=255.0):
    """Where a light band lies across `prof`, and how wide it is.

    The two flanks come from the ends of the profile, so an edge with a lit
    plate on one side and a dark panel on the other -- which is every panel
    edge here -- is read without pretending the background is level."""
    p = np.asarray(prof, dtype=float)
    n = len(p)
    lo = np.arange(n, dtype=float) + p0
    hi = lo + 1.0
    left, right = p[0], p[-1]
    # start from the ink's own first moment and mass rather than from the guess
    # the caller had: this cost has a local minimum one pixel out on either
    # side, and a band split evenly across a pixel boundary sits between them
    base = np.linspace(left, right, n)
    ex = np.clip(p - base, 0, None)
    if ex.sum() > 1e-6:
        c = p0 + float((ex * (np.arange(n) + .5)).sum() / ex.sum())
        w = float(ex.sum() / max(ink - base.mean(), 1.0))
        a0, w0 = c - w / 2, float(np.clip(w, .2, 2.5))

    def cost(q):
        if q[1] < .15 or q[1] > 3.0:
            return 1e9
        a, b = q[0], q[0] + q[1]
        band = np.clip(np.minimum(hi, b) - np.maximum(lo, a), 0, None)
        pre = np.clip(np.minimum(hi, a) - lo, 0, None)
        post = np.clip(hi - np.maximum(lo, b), 0, None)
        return float(((pre * left + band * ink + post * right - p) ** 2).sum())

    q, err = descend([a0, w0], cost, [.2, .15])
    return q[0], q[1], err


def fit_ring(lum, g0, along, half=3, ink=255.0, per=None):
    """The four sides of a panel: the light band that draws each, as the
    rounded rect it bounds plus the one width they share.

    `per` narrows one side's window where a neighbouring line runs close: a
    window that reaches past its own band reads the neighbour as a flank."""
    xa, ya = along
    got, ws = {}, []
    for key, axis, at, ts, take in (("x0", "h", g0[0], ya, 0), ("x1", "h", g0[1], ya, 1),
                                    ("y0", "v", g0[2], xa, 0), ("y1", "v", g0[3], xa, 1)):
        h = (per or {}).get(key, half)
        a0 = int(round(at)) - h
        vals = []
        for t in ts:
            p = lum[t, a0:a0 + 2 * h + 1] if axis == "h" else lum[a0:a0 + 2 * h + 1, t]
            a, w, _ = band_fit(p, a0, at, .9, ink)
            vals.append((a, w))
        a = float(np.median([v[0] for v in vals]))
        w = float(np.median([v[1] for v in vals]))
        got[key] = a if take == 0 else a + w
        ws.append(w)
    return [got["x0"], got["x1"], got["y0"], got["y1"]], float(np.median(ws))


def ring_radius(lum, g, w, inside, outside, r0, ink=255.0, pad=1.5):
    """A panel's corner radius, by rendering its four corners back.

    A band fitted row by row cannot see a radius: near the tangent the edge
    moves quadratically, so a threshold reads every radius as the same one.
    The whole corner is modelled instead -- outside, ring, inside -- and the
    radius is what makes the four of them land."""
    x0, x1, y0, y1 = g[:4]
    wins = []
    for cx, cy, sx, sy in ((x0, y0, +1, +1), (x1, y0, -1, +1),
                           (x1, y1, -1, -1), (x0, y1, +1, -1)):
        span = r0 + 3
        xa, xb = (cx - pad, cx + span) if sx > 0 else (cx - span, cx + pad)
        ya, yb = (cy - pad, cy + span) if sy > 0 else (cy - span, cy + pad)
        win = Win(xa, xb, ya, yb)
        wins.append((win, win.crop(lum), win.crop(inside), win.crop(outside)))

    def cost(p):
        r = p[0]
        if r < 1.5 or r > 22 or r > min(x1 - x0, y1 - y0) / 2:
            return 1e9
        e = 0.0
        for win, tgt, ins, out in wins:
            d = sdf(win.px, win.py, [x0, x1, y0, y1, r])
            body = down((d <= 0).astype(float))
            band = down(((d <= 0) & (d >= -w)).astype(float))
            model = out + body * (ins - out) + band * (ink - ins)
            e += float(((model - tgt) ** 2).mean())
        return e

    p, err = descend([r0], cost, [.5])
    return p[0], err / len(wins)


def fit_rim(lum, base_lum, g):
    """A plate's edge dressing: the pale line just inside it, and the dark line
    on its outermost pixel.

    White and black at an opacity rather than at a colour, because the ramp
    under them runs from near-black to cyan and no fixed colour sits on both.
    The pale line is not one weight all the way round -- across the top and
    bottom it is about three times what it is down the sides -- so it is fitted
    twice and drawn as one ring plus two bands.  The dark line is on the sides
    only: along the top and bottom the outermost pixel is just the edge's own
    coverage."""
    x0, x1, y0, y1, r = g
    vert, horz = [], []
    for t in range(int(y0 + r + 4), int(y1 - r - 4), 3):
        vert.append((lum[t, int(x0):int(x0) + 7], base_lum[t, int(x0) + 4], x0 - int(x0)))
        vert.append((lum[t, int(x1) - 6:int(x1) + 1][::-1], base_lum[t, int(x1) - 4],
                     int(x1) + 1 - x1))
    for t in range(int(x0 + r + 4), int(x1 - r - 4), 3):
        horz.append((lum[int(y0):int(y0) + 7, t], base_lum[int(y0) + 4, t], y0 - int(y0)))
        horz.append((lum[int(y1) - 6:int(y1) + 1, t][::-1], base_lum[int(y1) - 4, t],
                     int(y1) + 1 - y1))

    def resid(samples, t, w, o, od, wd):
        e = []
        for prof, base, off in samples:
            d0 = np.arange(len(prof), dtype=float) - off
            cov = np.clip(d0 + 1, 0, 1)
            rim = np.clip(np.minimum(d0 + 1, t + w) - np.maximum(d0, t), 0, None)
            drk = np.clip(np.minimum(d0 + 1, wd) - np.maximum(d0, 0), 0, None)
            model = (cov * base + rim * o * (255 - base) - drk * od * base
                     + (1 - cov) * BG)
            e.append(float(((model - prof) ** 2).sum()))
        # the worst third is dropped: the left plate's edge runs past three knob
        # glows and two tabs, and a plain sum would fit those instead
        return float(np.sort(e)[:int(len(e) * .67)].sum())

    def cost(p):
        t, w, ov, oh, od, wd = p
        if not (.2 < w < 4 and 0 <= ov <= 1 and 0 <= oh <= 1 and 0 <= od <= .6):
            return 1e9
        if not (0 <= t < 5 and .1 < wd < 2.5):
            return 1e9
        return resid(vert, t, w, ov, od, wd) + resid(horz, t, w, oh, 0.0, wd)

    p, err = descend([1.2, 1.4, .12, .3, .1, 1.0], cost, [.3, .3, .05, .05, .04, .2])
    return p, err / max((len(vert) + len(horz)) * 4, 1)


def azimuth(lum, cx, cy, rs, a0, a1):
    """The median luminance at each radius, over one angular sector of a knob."""
    out = []
    for r in rs:
        vals = []
        for a in np.arange(a0, a1, 2.0):
            x = cx + r * np.cos(np.radians(a))
            y = cy - r * np.sin(np.radians(a))
            ix, iy = int(x - .5), int(y - .5)
            fx, fy = x - .5 - ix, y - .5 - iy
            vals.append((1 - fx) * (1 - fy) * lum[iy, ix] + fx * (1 - fy) * lum[iy, ix + 1]
                        + (1 - fx) * fy * lum[iy + 1, ix] + fx * fy * lum[iy + 1, ix + 1])
        out.append(np.median(vals))
    return np.asarray(out)


# ------------------------------------------------------------- gradients ---
def ramp_stops(vs, cols, tol=0.9):
    """Piecewise-linear stops holding `cols(vs)` to within `tol` per channel.

    Greedy from the left: extend a segment while its chord stays inside
    tolerance.  A fixed stop count would spend most of itself on the flat run
    these ramps open with."""
    vs = np.asarray(vs, dtype=float)
    cols = np.asarray(cols, dtype=float)
    keep, i = [0], 0
    while i < len(vs) - 1:
        j = len(vs) - 1
        while j > i + 1:
            t = ((vs[i:j + 1] - vs[i]) / (vs[j] - vs[i]))[:, None]
            if np.abs(cols[i] + t * (cols[j] - cols[i]) - cols[i:j + 1]).max() <= tol:
                break
            j -= 1
        keep.append(j)
        i = j
    return [(float(vs[k]), cols[k]) for k in keep]


def profile(img, mask, sign, step=3.0, tol=0.9, flat=None):
    """A plate's ramp, as a median over each band of x-y (sign +1) or y-x (-1).

    A median rather than a mean because the strip the left plate can be read
    from still has the knobs and the wordmark in it."""
    H, W = mask.shape
    ys, xs = np.mgrid[0:H, 0:W]
    v = (sign * (xs - ys))[mask]
    c = img[mask]
    k = np.round((v - v.min()) / step).astype(int)
    vs, cols = [], []
    for kk in range(k.max() + 1):
        s = k == kk
        if s.sum() < 12:
            continue
        vs.append(float(v.min() + kk * step))
        cols.append(np.median(c[s], axis=0))
    if flat is not None:
        vs, cols = [flat[0]] + vs, [np.asarray(flat[1], float)] + cols
    return ramp_stops(vs, cols, tol)


def grad_def(gid, stops, sign, scale=1.0, base=None):
    """A userSpaceOnUse linearGradient along x-y (sign +1) or y-x (sign -1).

    Through (s*v0/2, -s*v0/2) and (s*v1/2, -s*v1/2) the gradient parameter of
    any point works out as exactly (s*(x-y) - v0) / (v1 - v0)."""
    v0, v1 = stops[0][0], stops[-1][0]
    zero = np.asarray(stops[0][1], dtype=float)
    root = zero if base is None else np.asarray(base, dtype=float)
    body = "".join('<stop offset="%s" stop-color="%s"/>'
                   % (fmt((v - v0) / (v1 - v0), 4),
                      hexc(_i(root + scale * (np.asarray(c, dtype=float) - zero))))
                   for v, c in stops)
    return ('<linearGradient id="%s" gradientUnits="userSpaceOnUse" '
            'x1="%s" y1="%s" x2="%s" y2="%s">%s</linearGradient>'
            % (gid, fmt(sign * v0 / 2), fmt(-sign * v0 / 2),
               fmt(sign * v1 / 2), fmt(-sign * v1 / 2), body))


def wash(img, mask, stops, sign):
    """How much of the plate's ramp shows through a panel, and over what base.

    The panels are a dark wash rather than an opaque fill, so a panel's colour
    is base + k*(ramp(v) - ramp(v0)); both fall out of one least squares."""
    H, W = mask.shape
    ys, xs = np.mgrid[0:H, 0:W]
    v = (sign * (xs - ys))[mask].astype(float)
    c = img[mask]
    sv = np.asarray([s[0] for s in stops], dtype=float)
    sc = np.asarray([s[1] for s in stops], dtype=float)
    ref = np.stack([np.interp(v, sv, sc[:, i]) for i in range(3)], axis=1) - sc[0]
    rc, cc = ref - ref.mean(axis=0), c - c.mean(axis=0)
    den = float((rc * rc).sum())
    k = 0.0 if den < 1e-9 else float((rc * cc).sum() / den)
    return k, c.mean(axis=0) - k * ref.mean(axis=0)


# ------------------------------------------------------------------ text ---
def ink_of(img, box, bg, ink):
    """Per-pixel text coverage. `bg` may be a whole-canvas plate, because the
    wordmark sits on the ramp and a level background would read it as ink."""
    x0, x1, y0, y1 = box
    sub = img[y0:y1, x0:x1].mean(axis=2)
    b = bg if np.isscalar(bg) else bg[y0:y1, x0:x1]
    return np.clip((sub - b) / (ink - b), 0, 1)


def stroke_width(mask):
    """Mean ink across a stroke: total coverage over the number of stems it
    crosses.  Two faces can carry the same ink at different sizes, and this is
    what tells them apart -- it is the thing that reads as a different face."""
    tot = runs = 0.0
    for row in np.asarray(mask):
        on = row > .35
        tot += float(row.sum())
        runs += int(np.count_nonzero(on[1:] & ~on[:-1]) + (1 if on[0] else 0))
    return tot / max(runs, 1)


def fit_text(img, box, bg, ink, weight, word, tag, p0):
    """Cap height, baseline and centre, by rendering candidates at the mask.

    Restarted from several cap heights: a coordinate descent on a rendered
    glyph run is not convex -- one size out, every stem lands between the old
    ones and the residual goes back up, so a single start settles wherever it
    began."""
    x0, x1, y0, y1 = box
    target = ink_of(img, box, bg, ink)
    scratch = os.path.join(TMP, "bg_" + tag)

    def cost(p):
        if p[0] < 2 or p[0] > 20:
            return 1e9
        d = text_d(weight, word, p[0], p[1] - y0, p[2] - x0)
        got = svgify.render_text_only(d, y1 - y0, x1 - x0, scratch)
        return float(np.abs(got - target).sum())

    best = None
    for dc in (-2.0, -1.0, 0.0, 1.0, 2.0):
        p, err = descend([p0[0] + dc, p0[1], p0[2]], cost, [.4, .4, .4])
        if best is None or err < best[1]:
            best = (p, err)
    p, err = best
    d = text_d(weight, word, p[0], p[1] - y0, p[2] - x0)
    got = svgify.render_text_only(d, y1 - y0, x1 - x0, scratch)
    return (dict(cap=p[0], baseline=p[1], centre=p[2]),
            err / max(target.sum(), 1), got.sum() / max(target.sum(), 1e-6),
            stroke_width(got) / max(stroke_width(target), 1e-6))


# ---------------------------------------------------------- render back ---
def svg(defs, body):
    head = ('<svg xmlns="http://www.w3.org/2000/svg" width="563" height="376" '
            'viewBox="0 0 563 376">')
    return "\n".join([head, "<defs>" + "".join(defs) + "</defs>"] + body + ["</svg>"]) + "\n"


REGIONS = dict(surround=None,
               left_column=(8, 71, 7, 370),
               panel_stack=(70, 200, 7, 370),
               main_display=(209, 556, 7, 370),
               logo_area=(8, 71, 288, 362))


def compare():
    subprocess.run(["rsvg-convert", "-w", "563", "-h", "376", SVG,
                    "-o", os.path.join(TMP, "cmp_01.png")], check=True)
    got = np.asarray(Image.open(os.path.join(TMP, "cmp_01.png")).convert("RGB"),
                     dtype=np.float64)
    png = load()
    e = np.abs(got - png)
    out = [("overall", float(e.mean()), float(e.max()))]
    for name, box in REGIONS.items():
        if box is None:
            continue
        x0, x1, y0, y1 = box
        s = e[y0:y1, x0:x1]
        out.append((name, float(s.mean()), float(s.max())))
    return out, got


# ============================================================ what to fit ===
# start boxes and where to read each side from: xs along the horizontal edges,
# ys along the vertical ones, both kept clear of the corners and of whatever
# else runs near.  `half` widens or narrows a side's window where a neighbour
# is close -- the column frame's line sits two pixels above the first panel's.
PANELS = [
    dict(tag="A", box=[75.3, 190.3, 13.5, 34.3], xs=[110, 140, 170], ys=[21, 24, 27],
         half=dict(y0=2), fill=(80, 186, 17, 31), r0=6.0),
    dict(tag="B", box=[75.5, 190.5, 39.0, 74.4], xs=[110, 140, 170], ys=[48, 56, 64],
         fill=(80, 186, 44, 70), r0=6.0, blue=True),
    dict(tag="C", box=[75.7, 190.7, 79.2, 156.2], xs=[110, 140, 170], ys=[95, 120, 145],
         fill=(80, 186, 84, 152), r0=6.0),
    dict(tag="D", box=[75.5, 190.3, 174.2, 288.2], xs=[95, 105, 112], ys=[200, 240, 270],
         fill=(80, 186, 180, 244), r0=6.0),
    dict(tag="E", box=[75.4, 190.5, 306.7, 326.8], xs=[110, 140, 170], ys=[313, 317, 321],
         fill=(80, 186, 311, 323), r0=5.0),
    dict(tag="F", box=[71.2, 194.8, 334.6, 365.7], xs=[110, 140, 170], ys=[345, 352, 358],
         half=dict(y0=2), fill=(78, 190, 339, 362), r0=6.0),
]

LABELS = [
    dict(tag="in", word="in", box=(38, 56, 22, 38), fill=BLUE, ink=144.0,
         group="knob", p0=(8.2, 34.0, 46.0)),
    dict(tag="out", word="out", box=(32, 60, 95, 111), fill=BLUE, ink=144.0,
         group="knob", p0=(8.2, 107.0, 45.0)),
    dict(tag="mix", word="mix", box=(32, 60, 170, 187), fill=BLUE, ink=144.0,
         group="knob", p0=(8.2, 183.0, 46.0)),
    dict(tag="lfo", word="LFO", box=(77, 106, 160, 177), fill="#fff", ink=255.0,
         group="panel", p0=(9.0, 173.0, 90.0)),
    dict(tag="ext", word="External audio", box=(77, 172, 289, 305), fill="#fff",
         ink=255.0, group="panel", p0=(8.0, 302.0, 124.0)),
    dict(tag="tm", word="TM", box=(50, 66, 294, 306), fill="#fff", ink=255.0,
         group="panel", p0=(4.3, 302.0, 58.0)),
    # the wordmark's fill and ink are the artwork's own, read off the ramp it
    # sits on, so they are not spelled out here
    dict(tag="Spectrum", word="Spectrum", box=(12, 64, 327, 345), fill=None,
         ink=None, group="wordmark", p0=(8.0, 340.0, 37.5)),
    dict(tag="Worx", word="Worx", box=(12, 64, 341, 358), fill=None,
         ink=None, group="wordmark", p0=(8.0, 353.0, 38.0)),
]


def build():
    img = load()
    lum = img.mean(axis=2)
    H, W = lum.shape
    ys, xs = np.mgrid[0:H, 0:W]
    note, defs, body = [], [], []

    # ------------------------------------------------------------- ramps --
    m = np.zeros((H, W), bool)
    m[11:368, 10:69] = True
    for cy in (63, 136, 212):                       # the knobs and their rings
        m &= np.hypot(xs + .5 - 46.5, ys + .5 - cy) > 37
    m[290:362, 14:68] = False                       # the logo and the wordmark
    lstops = profile(img, m, LEFT, flat=(-60.0, (35, 31, 32)))
    m = np.zeros((H, W), bool)
    m[14:362, 218:547] = True
    mstops = profile(img, m, MAIN)

    def ramp(stops, sign, x, y):
        v = sign * (np.asarray(x) - np.asarray(y))
        sv = np.asarray([s[0] for s in stops])
        sc = np.asarray([s[1] for s in stops])
        return np.stack([np.interp(v, sv, sc[:, i]) for i in range(3)], axis=-1)

    lbase = ramp(lstops, LEFT, xs + .5, ys + .5).mean(axis=2)
    mbase = ramp(mstops, MAIN, xs + .5, ys + .5).mean(axis=2)
    note.append("ramps: left %d stops, main %d stops" % (len(lstops), len(mstops)))

    # ------------------------------------------------------------ plates --
    plates = {}
    for tag, (xa, xb, cols, rows, stops, sign, base) in dict(
            left=(8.7, 199.7, [40, 100, 150, 190], [60, 120, 300, 355], lstops, LEFT, lbase),
            main=(210.1, 554.9, [250, 350, 450, 540], [60, 120, 200, 300], mstops, MAIN, mbase),
    ).items():
        flat = float(np.asarray(stops[0][1]).mean())
        g = [outer_edge(lum, "h", xa, rows, True, flat),
             outer_edge(lum, "h", xb, rows, False, flat),
             outer_edge(lum, "v", 7.1, cols, True, flat),
             outer_edge(lum, "v", 369.1, cols, False, flat)]
        r, rerr = plate_radius(lum, g, flat)
        g = g + [r]
        rim, rimerr = fit_rim(lum, base, g)
        plates[tag] = dict(g=g, rim=rim, stops=stops, sign=sign)
        note.append("%s plate %s r=%.2f (%.4f)  rim in %.2f w %.2f side %.3f "
                    "ends %.3f  dark %.3f w %.2f (%.2f)"
                    % ((tag, ["%.2f" % v for v in g[:4]], r, rerr) + tuple(rim) + (rimerr,)))

    # -------------------------------------------------------------- tabs --
    tabs = []
    for y0 in (64.8, 305.8):
        t0 = outer_edge(lum, "v", y0, [203, 206], True, 25.0)
        t1 = outer_edge(lum, "v", y0 + 21, [203, 206], False, 25.0)
        tabs.append((t0, t1))
    tab_rgb = _i(np.median(img[68:82, 201:208].reshape(-1, 3), axis=0))
    note.append("tabs y %.2f..%.2f and %.2f..%.2f  %s"
                % (tabs[0][0], tabs[0][1], tabs[1][0], tabs[1][1], hexc(tab_rgb)))

    # ------------------------------------------------- the module column --
    fg, fw = fit_ring(lum, [70.9, 194.6, 11.5, 330.3], ([100, 140, 170], [100, 200, 300]),
                      half=3)
    # the top runs two pixels above the first panel's own line: fix the width
    # from the other three sides and let only the position move
    p = lum[9:14, 140]
    a, _, _ = band_fit(p, 9, 11.5, fw)
    fg[2] = a
    m = np.zeros((H, W), bool)
    m[16:328, 72:74] = True
    m[16:328, 192:194] = True
    fk, fbase = wash(img, m, lstops, LEFT)
    fmap = wash_map(lstops, fbase, fk, xs, ys)
    fr, frerr = ring_radius(lum, fg, fw, fmap, lbase, 7.0)
    note.append("column %s r=%.2f w=%.2f (%.4f)  fill %s + %.3f ramp"
                % (["%.2f" % v for v in fg], fr, fw, frerr, hexc(_i(fbase)), fk))

    # ------------------------------------------------------------ panels --
    fitted = []
    for spec in PANELS:
        ink = 190.0 if spec.get("blue") else 255.0
        g, w = fit_ring(lum, spec["box"], (spec["xs"], spec["ys"]),
                        half=3, ink=ink, per=spec.get("half", {}))
        x0, x1, y0, y1 = spec["fill"]
        m = np.zeros((H, W), bool)
        m[y0:y1, x0:x1] = True
        if spec["tag"] == "D":
            m[246:276, 143:182] = False              # the button recess
        k, base = wash(img, m, lstops, LEFT)
        pmap = wash_map(lstops, base, k, xs, ys)
        out = lbase if spec["tag"] == "F" else fmap
        r, rerr = ring_radius(lum, g, w, pmap, out, spec["r0"], ink=ink)
        fitted.append(dict(spec, g=g + [r], w=w, k=k, base=base, map=pmap, out=out))
        note.append("panel %s %s r=%.2f w=%.2f (%.4f)  fill %s + %.3f ramp"
                    % (spec["tag"], ["%.2f" % v for v in g], r, w, rerr,
                       hexc(_i(base)), k))
    return dict(img=img, lum=lum, note=note, lstops=lstops, mstops=mstops,
                lbase=lbase, mbase=mbase, ramp=ramp, plates=plates, tabs=tabs,
                tab_rgb=tab_rgb, fmap=fmap,
                frame=dict(g=fg + [fr], w=fw, k=fk, base=fbase),
                panels=fitted)


def notch_feet(lum, ylo, yhi, w):
    """Where the LFO panel's stepped top edge leaves one level for the other.

    Column by column: the top edge's band is found from its own brightest row,
    so the two feet are the last column still at the low level and the first
    back at the high one, with no assumption about the shape between them."""
    lo, hi = int(yhi) - 4, int(ylo) + 5
    obs = []
    for x in range(108, 152):
        p = lum[lo:hi, x]
        a, _, _ = band_fit(p, lo, lo + float(np.argmax(p)), w)
        obs.append((x + .5, a))
    xs_ = np.asarray([o[0] for o in obs])
    ys_ = np.asarray([o[1] for o in obs])
    # two quarter arcs meeting halfway have to share a radius, and the drop
    # between the levels already fixes it: only where the step sits is free
    r = (ylo - yhi) / 2

    def cost(q):
        xm = q[0]
        xa, xb = xm - r, xm + r
        model = np.where(xs_ <= xa, ylo,
                         np.where(xs_ <= xm,
                                  ylo - r + np.sqrt(np.clip(r * r - (xs_ - xa) ** 2, 0, None)),
                                  np.where(xs_ <= xb,
                                           yhi + r - np.sqrt(np.clip(r * r - (xs_ - xb) ** 2, 0, None)),
                                           yhi)))
        return float(((model - ys_) ** 2).sum())

    q, err = descend([127.0], cost, [.5])
    return q[0] - r, q[0] + r, r, err / len(obs)


def fit_glow(lum, base, g, w, dists=(1, 2, 3)):
    """The halo outside a ring, as the alpha each band carries.

    Median over the four sides and many positions along them, because the
    panels sit close enough that one side's samples reach the next panel's
    line -- a mean would carry that in, a median does not."""
    x0, x1, y0, y1, r = g[0], g[1], g[2], g[3], g[4]
    ts = list(range(int(y0 + r + 3), int(y1 - r - 3), 3))
    us = list(range(int(x0 + r + 3), int(x1 - r - 3), 3))
    out = []
    for d in dists:
        sides = [[(lum[t, int(np.floor(x0)) - d] - base[t, int(np.floor(x0)) - d])
                  / max(255 - base[t, int(np.floor(x0)) - d], 1e-6) for t in ts],
                 [(lum[t, int(np.ceil(x1)) + d - 1] - base[t, int(np.ceil(x1)) + d - 1])
                  / max(255 - base[t, int(np.ceil(x1)) + d - 1], 1e-6) for t in ts],
                 [(lum[int(np.floor(y0)) - d, u] - base[int(np.floor(y0)) - d, u])
                  / max(255 - base[int(np.floor(y0)) - d, u], 1e-6) for u in us],
                 [(lum[int(np.ceil(y1)) + d - 1, u] - base[int(np.ceil(y1)) + d - 1, u])
                  / max(255 - base[int(np.ceil(y1)) + d - 1, u], 1e-6) for u in us]]
        a = float(np.clip(min(np.median(v) for v in sides if len(v)), 0, 1))
        out.append(a if not out else min(a, out[-1]))
    bands, below = [], 0.0
    for i in range(len(dists) - 1, -1, -1):          # stacked, largest first
        a = out[i]
        bands.append((float(dists[i]), 0.0 if a <= below + 1e-4
                      else (a - below) / max(1 - below, 1e-6)))
        below = max(below, a)
    return [(e, o) for e, o in bands if o > .004], out


def fit_knob(lum, base, cx, cy):
    """A knob: the black disc, and the bright arc that lights its lower left.

    The disc's radius is where its coverage crosses a half, read over the left
    half only -- the artwork's edge is soft, so counting the pixels that reach
    black reads it a pixel small, and the right half is under the module column.
    The ring is not a ring: it is an arc, absent across the top and buried under
    the column on the right, so its radius, profile and angular reach are all
    read from where it actually is."""
    plate = float(base[int(cy), int(cx) - 34])
    rr_ = np.arange(24, 34, .25)
    dark = np.clip((plate - azimuth(lum, cx, cy, rr_, 100, 260)) / plate, 0, 1)
    i = int(np.argmax(dark < .5))
    R = float(np.interp(.5, [dark[i], dark[i - 1]], [rr_[i], rr_[i - 1]]))

    rs = np.arange(max(R - 4, 2), R + 8, .25)
    prof = azimuth(lum, cx, cy, rs, 150, 345) - plate
    ra = float(rs[int(np.argmax(prof))])
    A = np.stack([np.clip(np.minimum(rs + .5, ra + wd / 2)
                          - np.maximum(rs - .5, ra - wd / 2), 0, None)
                  for wd in (1.2, 2.6, 4.6)], axis=1)
    v = np.linalg.lstsq(A, np.clip(prof, 0, None) / (255 - plate), rcond=None)[0]

    # how far round it goes: the run of angles still carrying most of the peak
    angs = np.arange(0, 360, 3.0)
    ring = azimuth(lum, cx, cy, [ra], 0, 1) * 0
    ring = np.asarray([azimuth(lum, cx, cy, [ra], a, a + 3)[0] for a in angs]) - plate
    peak = float(ring[(angs > 170) & (angs < 320)].max())
    k = int(np.argmax(ring * ((angs > 170) & (angs < 320))))
    a0 = a1 = k
    while ring[(a0 - 1) % len(angs)] > peak * .3:
        a0 -= 1
    while ring[(a1 + 1) % len(angs)] > peak * .3:
        a1 += 1
    return dict(cx=cx, cy=cy, R=R, ra=ra, a0=float(angs[a0 % len(angs)]),
                a1=float(angs[a1 % len(angs)] + (360 if a1 >= len(angs) else 0)),
                arcs=[(wd, float(np.clip(o, 0, 1)))
                      for wd, o in zip((1.2, 2.6, 4.6), v) if o > .012])


def fit_logo(img):
    """The swash, as the two stroked paths it is drawn with.

    The centre line of each stroke is its ink's first moment and the width is
    that ink's mass, both read off the artwork; only the claim that the curves
    are circular arcs is a model."""
    r = img[:, :, 0]
    ink = 245.0

    def cov(y0, y1, x0, x1):
        sub = r[y0:y1, x0:x1]
        bg = np.percentile(sub, 10)
        return np.clip((sub - bg) / (ink - bg), 0, 1)

    def cx_of(y0, y1, x0, x1):
        c = cov(y0, y1, x0, x1).mean(axis=0)
        return x0 + float((c * (np.arange(len(c)) + .5)).sum() / c.sum()), float(c.sum())

    def cy_of(y0, y1, x0, x1):
        c = cov(y0, y1, x0, x1).mean(axis=1)
        return y0 + float((c * (np.arange(len(c)) + .5)).sum() / c.sum()), float(c.sum())

    mid, w = cx_of(306, 313, 32, 40)
    uleft, _ = cx_of(306, 313, 25, 32)
    uright, _ = cx_of(306, 313, 39, 46)
    bar, _ = cy_of(310, 319, 33, 39)
    utop, _ = cy_of(301, 309, 28, 31)
    bowl_x, _ = cx_of(316, 319, 17, 25)
    bowl_y, _ = cy_of(319, 326, 26, 33)
    arch_y, _ = cy_of(294, 301, 40, 46)
    arch_x, _ = cx_of(301, 305, 45, 53)
    r1 = (mid - bowl_x) / 2
    r2 = (arch_x - mid) / 2
    return dict(w=w, mid=mid, uleft=uleft, uright=uright, bar=bar,
                utop=utop - w / 2, bowl=(bowl_x, bowl_y - r1, r1),
                arch=(arch_x, arch_y + r2, r2))


# ============================================================ the drawing ===
def band(x0, x1, y0, y1):
    """One axis-aligned rectangle, as a subpath."""
    f = fmt
    return "M%s %sH%sV%sH%sZ" % (f(x0), f(y0), f(x1), f(y1), f(x0))


def emit(s):
    lstops, mstops = s["lstops"], s["mstops"]
    defs = [grad_def("l", lstops, LEFT), grad_def("m", mstops, MAIN)]
    body = ['<path d="M0 0H563V376H0Z" fill="#e6e6e6"/>']

    tab = "".join(band(s["tabx"], 211.0, t0, t1) for t0, t1 in s["tabs"])
    plate = s["plates"]
    body.append('<path d="%s" fill="url(#l)"/>' % rr_path(plate["left"]["g"]))
    for tag in ("left", "main"):
        gg = plate[tag]["g"]
        t, w, ov, oh, od, wd = plate[tag]["rim"]
        x0, x1, y0, y1, r = gg
        if od > .004:
            body.append('<path d="%s" fill="#000" fill-opacity="%s"/>'
                        % ("".join(band(a, a + wd, y0 + r, y1 - r)
                                   for a in (x0, x1 - wd)), fmt(od, 3)))
        body.append('<path d="%s%s" fill="#fff" fill-opacity="%s" fill-rule="evenodd"/>'
                    % (rr_path(gg, t), rr_path(gg, t + w), fmt(ov, 3)))
        extra = (oh - ov) / max(1 - ov, 1e-6)
        if extra > .004:
            body.append('<path d="%s" fill="#fff" fill-opacity="%s"/>'
                        % ("".join(band(x0 + r, x1 - r, b, b + w)
                                   for b in (y0 + t, y1 - t - w)), fmt(extra, 3)))
        if tag == "left":
            body.append('<path d="%s" fill="%s"/>' % (tab, hexc(s["tab_rgb"])))
            body.append('<path d="%s" fill="url(#m)"/>' % rr_path(plate["main"]["g"]))

    def wash_fill(gid, k, base, span):
        """A flat fill where the ramp never reaches, the scaled ramp where it does."""
        lo, hi = span
        v = [x[0] for x in lstops]
        c = np.asarray([x[1] for x in lstops])
        reach = np.interp(min(hi, v[-1]), v, c[:, 1]) - c[0][1]
        if abs(k) * reach < 1.5:
            return hexc(_i(base))
        cut = ramp_stops(v, c, tol=.9 / max(abs(k), .05))
        defs.append(grad_def(gid, cut, LEFT, scale=k, base=base))
        return "url(#%s)" % gid

    def panel(gid, path_of, g, w, k, base, glow, ring="#fff"):
        for e, o in glow:
            body.append('<path d="%s" fill="#fff" fill-opacity="%s"/>'
                        % (path_of(g, -e), fmt(o, 3)))
        span = (LEFT * (g[0] - g[3]), LEFT * (g[1] - g[2]))
        body.append('<path d="%s" fill="%s"/>'
                    % (path_of(g, 0.0), wash_fill(gid, k, base, (min(span), max(span)))))
        body.append('<path d="%s%s" fill="%s" fill-rule="evenodd"/>'
                    % (path_of(g, 0.0), path_of(g, w), ring))

    f = s["frame"]
    panel("c", rr_path, f["g"], f["w"], f["k"], f["base"], f["glow"])
    # the knobs run under the panels and over the column's own frame, which is
    # the order the artwork has: the frame's line is blacked out where a knob
    # crosses it and the panels' lines are not
    for k in s["knobs"]:
        body.append('<path d="%s" fill="#000"/>' % circ_path(k["cx"], k["cy"], k["R"]))
        d = arc_path(k["cx"], k["cy"], k["ra"], k["a0"], k["a1"])
        for wd, o in k["arcs"]:
            body.append('<path d="%s" fill="none" stroke="#fff" stroke-width="%s" '
                        'stroke-opacity="%s"/>' % (d, fmt(wd), fmt(o, 3)))
    for p in s["panels"]:
        if p["tag"] == "D":
            step = s["notch"]
            g = p["g"][:4] + [p["g"][4]] * 4      # tr, br, bl, and the low tl

            def path_of(gg, inset, _s=step):
                return notch_path(gg, _s, inset)
            panel("pd", path_of, g, p["w"], p["k"], p["base"], p["glow"])
            r = s["recess"]
            body.append('<path d="%s%s" fill="#fff" fill-rule="evenodd"/>'
                        % (rr_path(r["g"]), rr_path(r["g"], r["w"])))
            continue
        panel("p" + p["tag"].lower(), rr_path, p["g"], p["w"], p["k"], p["base"],
              p["glow"], ring=BLUE if p.get("blue") else "#fff")

    # the swash: one wave -- a half circle, a straight rise and an arch -- and
    # the "u" it sits over, both stroked at the width the artwork's ink carries
    lg = s["logo"]
    bx, by, br = lg["bowl"]
    ax, ay, ar = lg["arch"]
    f = fmt
    for d in (["M", f(bx), f(by), "A", f(br), f(br), "0", "0", "0", f(lg["mid"]), f(by),
               "V", f(ay), "A", f(ar), f(ar), "0", "0", "1", f(ax), f(ay)],
              ["M", f(lg["uleft"]), f(lg["utop"]), "V", f(lg["bar"]),
               "H", f(lg["uright"]), "V", f(lg["utop"])]):
        body.append('<path d="%s" fill="none" stroke="%s" stroke-width="%s"/>'
                    % (join(d), s["logo_fill"], fmt(lg["w"])))

    for tag, d, fill in s["text"]:
        body.append('<path d="%s" fill="%s"/>' % (d, fill))
    return svg(defs, body)


def wash_map(stops, base, k, xs, ys):
    v = LEFT * (xs + .5 - ys - .5)
    sv = np.asarray([x[0] for x in stops])
    sc = np.asarray([x[1] for x in stops])
    ref = np.stack([np.interp(v, sv, sc[:, i]) for i in range(3)], axis=-1) - sc[0]
    return (np.asarray(base, dtype=float) + k * ref).mean(axis=-1)


def finish(s):
    img, lum = s["img"], s["lum"]
    H, W = lum.shape
    ys, xs = np.mgrid[0:H, 0:W]
    note = s["note"]

    # the tab's left edge, against a row of the same plate without one
    ref = lum[95, 193:200]
    c = np.clip((ref - lum[70, 193:200]) / np.maximum(ref - 25.0, 1e-6), 0, 1)
    s["tabx"] = float(200 - c.sum())
    note.append("tab left edge %.2f" % s["tabx"])

    fmap = s["fmap"]
    s["frame"]["glow"], raw = fit_glow(lum, s["lbase"], s["frame"]["g"], s["frame"]["w"])
    note.append("column glow %s" % ["%.3f" % v for v in raw])
    for p in s["panels"]:
        base = s["lbase"] if p["tag"] == "F" else fmap
        p["glow"], raw = fit_glow(lum, base, p["g"], p["w"])
        note.append("panel %s glow %s" % (p["tag"], ["%.3f" % v for v in raw]))

    # ------------------------------------------------------- the LFO step --
    d = [p for p in s["panels"] if p["tag"] == "D"][0]
    yhi, _, _ = band_fit(lum[158:168, 175], 158, 162.0, d["w"])
    xa, xb, r, nerr = notch_feet(lum, d["g"][2], yhi, d["w"])
    s["notch"] = (xa, xb, yhi, r)
    note.append("LFO step x %.2f..%.2f  y %.2f (from %.2f)  r=%.2f (%.4f)"
                % (xa, xb, yhi, d["g"][2], r, nerr))

    rg, rw = fit_ring(lum, [146.5, 178.0, 251.0, 271.0], ([158, 165], [258, 262]))
    rr_, rerr = ring_radius(lum, rg, rw, d["map"], d["map"], 9.0)
    s["recess"] = dict(g=rg + [rr_], w=rw)
    note.append("recess %s r=%.2f w=%.2f (%.4f)" % (["%.2f" % v for v in rg], rr_, rw, rerr))

    # ---------------------------------------------------------- the knobs --
    dark = lum < 4
    rows = np.nonzero(dark.any(axis=1))[0]
    runs, i = [], 0
    while i < len(rows):
        j = i
        while j + 1 < len(rows) and rows[j + 1] == rows[j] + 1:
            j += 1
        runs.append((rows[i], rows[j]))
        i = j + 1
    s["knobs"] = []
    for r0, r1 in runs:
        m = dark[r0:r1 + 1]
        yy, xx = np.nonzero(m)
        k = fit_knob(lum, s["lbase"], float(xx.mean() + .5), float(r0 + yy.mean() + .5))
        s["knobs"].append(k)
        note.append("knob %.2f,%.2f  R=%.2f  arc r=%.2f %.0f..%.0f deg %s"
                    % (k["cx"], k["cy"], k["R"], k["ra"], k["a0"], k["a1"],
                       ["%.1f/%.3f" % a for a in k["arcs"]]))

    # ----------------------------------------------------------- the logo --
    s["logo"] = fit_logo(img)
    s["logo_fill"] = hexc(_i(np.percentile(img[305:313, 34:38].reshape(-1, 3), 90, axis=0)))
    note.append("logo stroke %.2f  bowl %s  arch %s  fill %s"
                % (s["logo"]["w"], ["%.2f" % v for v in s["logo"]["bowl"]],
                   ["%.2f" % v for v in s["logo"]["arch"]], s["logo_fill"]))

    # ----------------------------------------------------------- the text --
    # The weight is decided per group rather than per word, and by ink mass
    # rather than by L1: at these sizes L1 prefers whichever face lays down
    # less, and a run of three labels that came out in three different weights
    # would be wrong however good each one's number looked.
    plate_flat = float(np.asarray(s["lstops"][0][1]).mean())
    wm = np.percentile(img[331:340, 16:60].reshape(-1, 3), 97, axis=0)
    fits, mass = {}, {}
    for spec in LABELS:
        if spec["group"] == "wordmark":
            bg, ink, fill = s["lbase"], float(np.asarray(_i(wm)).mean()), hexc(_i(wm))
        elif spec["fill"] == "#fff":
            bg, ink, fill = fmap[int(spec["box"][2]), int(spec["box"][0])], 255.0, "#fff"
        else:
            bg, ink, fill = plate_flat, spec["ink"], spec["fill"]
        spec["resolved"] = fill
        for weight in ("bold", "reg"):
            p, err, m, sw = fit_text(img, spec["box"], bg, ink, weight, spec["word"],
                                     spec["tag"], spec["p0"])
            fits[(spec["tag"], weight)] = p
            mass.setdefault((spec["group"], weight), []).append((m, sw))
            note.append("  %-14s %-4s cap %.2f base %.2f centre %.2f  "
                        "L1 %.3f mass %.3f stroke %.3f"
                        % (spec["tag"], weight, p["cap"], p["baseline"], p["centre"],
                           err, m, sw))
    chosen = {}
    for group in dict.fromkeys(spec["group"] for spec in LABELS):
        def score(w, _g=group):
            return sum(abs(np.log(max(m, 1e-6))) + abs(np.log(max(sw, 1e-6)))
                       for m, sw in mass[(_g, w)])
        chosen[group] = min(("bold", "reg"), key=score)
        note.append("%-16s %s  (bold %.3f, reg %.3f)"
                    % (group, chosen[group], score("bold"), score("reg")))
    s["text"] = []
    for spec in LABELS:
        weight = chosen[spec["group"]]
        p = fits[(spec["tag"], weight)]
        s["text"].append((spec["tag"], text_d(weight, spec["word"], p["cap"],
                                              p["baseline"], p["centre"]),
                          spec["resolved"]))
        note.append("%-16s %-4s cap %.2f base %.2f centre %.2f %s"
                    % (spec["tag"], weight, p["cap"], p["baseline"], p["centre"],
                       spec["resolved"]))
    return s


def main():
    FACES["bold"] = face("VeraBd.ttf")
    FACES["reg"] = face("Vera.ttf")
    s = finish(build())
    with open(SVG, "w") as fh:
        fh.write(emit(s))
    for line in s["note"]:
        print(line)
    print()
    rows, _ = compare()
    for name, mean, mx in rows:
        print("%-14s delta mean %5.2f/255  max %6.2f" % (name, mean, mx))
    print("\nsvg %d B   png %d B" % (os.path.getsize(SVG), os.path.getsize(PNG)))
    got = np.asarray(Image.open(os.path.join(TMP, "cmp_01.png")).convert("RGB"))
    col = np.unique(got[:, 562].reshape(-1, 3), axis=0)
    print("column 562: %s" % ("flat " + hexc(tuple(int(v) for v in col[0]))
                              if len(col) == 1 else "NOT FLAT (%d colours)" % len(col)))


if __name__ == "__main__":
    main()
