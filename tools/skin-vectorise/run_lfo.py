#!/usr/bin/env python3
"""Vectorise the LFO waveform icons, the LFO slider thumb and the two LEDs.

The waveforms (43-53) are one white stroked path each, described as what they
are -- a wave, straight segments for the triangle, saw and square, a fitted bell
for the exponent, a mitred spike for the dirac, ten circles for the random one
-- rather than traced off the bitmap. Every parameter (position, amplitude,
period, stroke width) is fitted by rendering the candidate with rsvg and
comparing premultiplied colour plus alpha against the PNG.

43 is the one that is not what it looks like. Its extremes are 9px apart and 9px
deep, but it passes through the middle three times as steeply as a sine of that
amplitude and period would, so it is the wave a designer draws -- one cubic per
half cycle, horizontal at the extremes -- with the handles fitted. They land at
80% of the half period where an exact sine wants 36.4%, and that exact sine
leaves 24.4/255 against 5.1 for the wave. main() re-measures both every run.

The originals were rasterised without antialiasing at 4x and box-downsampled
(every alpha in them is a multiple of 255/16), which is why the same pen comes
out 1.75px thick on one segment and 1.5px on the next: one pen, rounded to whole
4x subpixels by where it happened to land.

40 is the slider thumb, a 4x8 pill with the measured vertical ramp and a border
that is dark on the left and fades out to the right. 41/42 are the LED off/on
pair: one pill geometry, a soft glow that is concentric paths with falling
opacity (JUCE 8 has no <filter>), and different colours inside.
"""
import math
import os
import subprocess
from functools import lru_cache

import numpy as np
from PIL import Image

from svgify import SKIN, TMP, fmt, join, hexc

STROKE = "#fff"


# ------------------------------------------------------------------ render --
def render(svg, W, H, tag):
    """Rasterise an SVG string at exactly W x H, as float RGBA in 0..1."""
    base = os.path.join(TMP, "lfo_" + tag)
    with open(base + ".svg", "w") as fh:
        fh.write(svg)
    subprocess.run(["rsvg-convert", "-w", str(W), "-h", str(H),
                    base + ".svg", "-o", base + ".png"], check=True)
    return np.asarray(Image.open(base + ".png").convert("RGBA"), dtype=np.float64) / 255.0


@lru_cache(maxsize=None)
def target(number):
    p = os.path.join(SKIN, "%02d.png" % number)
    return np.asarray(Image.open(p).convert("RGBA"), dtype=np.float64) / 255.0


def delta(got, want):
    """run_pills.verify's metric: premultiplied colour plus alpha, in 1/255."""
    gp = got[:, :, :3] * got[:, :, 3:4]
    wp = want[:, :, :3] * want[:, :, 3:4]
    return 255.0 * (np.abs(gp - wp).mean(axis=2) + np.abs(got[:, :, 3] - want[:, :, 3])).mean()


def fit(p0, steps, build, want, tag, bound=None):
    """Coordinate descent on the rendered delta. Halves the step until it is
    below a twentieth of a pixel, which is finer than the 4x grid the original
    was rasterised on."""
    H, W = want.shape[:2]
    cache = {}

    def cost(p):
        if bound and not bound(p):
            return 1e9
        svg = build(p)
        if svg not in cache:
            cache[svg] = delta(render(svg, W, H, tag), want)
        return cache[svg]

    best = list(p0)
    b = cost(best)
    steps = list(steps)
    while True:
        moved = False
        for i in range(len(best)):
            for s in (+steps[i], -steps[i]):
                c = list(best)
                c[i] += s
                v = cost(c)
                if v < b:
                    best, b, moved = c, v, True
        if not moved:
            steps = [s / 2 for s in steps]
            if max(steps) < 0.01:
                break
    return best, b


# -------------------------------------------------------------------- path --
def d_poly(pts):
    assert max(miter_ratios(pts), default=0) < 4, \
        "a join past the miter limit: rsvg bevels it, JUCE would not"
    toks = ["M", fmt(pts[0][0]), fmt(pts[0][1])]
    cur = pts[0]
    for x, y in pts[1:]:
        if fmt(y) == fmt(cur[1]):
            toks += ["H", fmt(x)]
        elif fmt(x) == fmt(cur[0]):
            toks += ["V", fmt(y)]
        else:
            toks += ["L", fmt(x), fmt(y)]
        cur = (x, y)
    return join(toks)


def hermite(f, df, xs):
    """Cubic Hermite through f at the given knots, as bezier control points:
    the value and the slope are exact at every knot."""
    segs = []
    for a, b in zip(xs, xs[1:]):
        h = (b - a) / 3.0
        segs.append(((a, f(a)), (a + h, f(a) + df(a) * h),
                     (b - h, f(b) - df(b) * h), (b, f(b))))
    return segs


def bez(seg, t):
    (x0, y0), (x1, y1), (x2, y2), (x3, y3) = seg
    u = 1 - t
    return (u * u * u * x0 + 3 * u * u * t * x1 + 3 * u * t * t * x2 + t * t * t * x3,
            u * u * u * y0 + 3 * u * u * t * y1 + 3 * u * t * t * y2 + t * t * t * y3)


def bez_split(seg, t):
    def mid(a, b):
        return (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t)
    p0, p1, p2, p3 = seg
    a, b, c = mid(p0, p1), mid(p1, p2), mid(p2, p3)
    d, e = mid(a, b), mid(b, c)
    f = mid(d, e)
    return (p0, a, d, f), (f, e, c, p3)


def bez_t_at_x(seg, x):
    lo, hi = 0.0, 1.0
    for _ in range(50):
        m = (lo + hi) / 2
        if bez(seg, m)[0] < x:
            lo = m
        else:
            hi = m
    return (lo + hi) / 2


def curve_error(segs, f):
    """Worst vertical deviation of the emitted beziers from the true curve."""
    e = 0.0
    for s in segs:
        for i in range(1, 8):
            x, y = bez(s, i / 8.0)
            e = max(e, abs(y - f(x)))
    return e


def d_segs(segs):
    toks = ["M", fmt(segs[0][0][0]), fmt(segs[0][0][1])]
    for _, c1, c2, p in segs:
        toks += ["C", fmt(c1[0]), fmt(c1[1]), fmt(c2[0]), fmt(c2[1]), fmt(p[0]), fmt(p[1])]
    return join(toks)


def knots(x0, x1, n, warp):
    """n+1 knots from x0 to x1, bunched towards x0 for warp > 1."""
    return [x0 + (x1 - x0) * (i / n) ** warp for i in range(n + 1)]


def fit_knots(f, df, x0, x1, warp, tol=0.03):
    n = 2
    while n < 64:
        segs = hermite(f, df, knots(x0, x1, n, warp))
        if curve_error(segs, f) < tol:
            return segs
        n *= 2
    return segs


def svg_stroke(W, H, d, w, extra=""):
    return ('<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d">\n'
            '<path d="%s" fill="none" stroke="%s" stroke-width="%s"%s/>\n'
            '</svg>\n' % (W, H, W, H, d, STROKE, fmt(w, 3), extra))


def miter_ratios(pts):
    """1/sin(theta/2) at every join -- over 4 and rsvg bevels where JUCE would
    still mitre, so the two renderers would disagree."""
    out = []
    for i in range(1, len(pts) - 1):
        a = np.array(pts[i - 1], float) - np.array(pts[i], float)
        c = np.array(pts[i + 1], float) - np.array(pts[i], float)
        a /= np.linalg.norm(a)
        c /= np.linalg.norm(c)
        half = math.acos(max(-1.0, min(1.0, float(np.dot(a, c))))) / 2
        out.append(1.0 / max(math.sin(half), 1e-6))
    return out


# ------------------------------------------------------------------ shapes --
W17, H17 = 24, 17


def wave_segs(xp, yp, P, D, k, x0, x1):
    """The wave as it was drawn: one cubic per half cycle, horizontal tangents
    at the extremes, handles k of the half period long. k = .3639 is a true
    sine; the artwork is steeper through the middle than that."""
    n0 = int(math.floor((x0 - xp) / P))
    n1 = int(math.ceil((x1 - xp) / P))
    ext = [(xp + i * P, yp + (D if i % 2 else 0)) for i in range(n0, n1 + 1)]
    segs = [((a[0], a[1]), (a[0] + k * P, a[1]), (b[0] - k * P, b[1]), (b[0], b[1]))
            for a, b in zip(ext, ext[1:])]
    segs[0] = bez_split(segs[0], bez_t_at_x(segs[0], x0))[1]
    segs[-1] = bez_split(segs[-1], bez_t_at_x(segs[-1], x1))[0]
    return segs


def b_sine(p):
    xp, yp, P, D, k, x0, x1, w = p
    return svg_stroke(W17, H17, d_segs(wave_segs(xp, yp, P, D, k, x0, x1)), w)


def b_truesine(p):
    """The same icon as a mathematically exact sine, for comparison."""
    xp, yp, P, D, x0, x1, w = p
    A, yc, kk = D / 2, yp + D / 2, math.pi / P
    f = lambda x: yc - A * math.cos(kk * (x - xp))
    df = lambda x: A * kk * math.sin(kk * (x - xp))
    return svg_stroke(W17, H17, d_segs(fit_knots(f, df, x0, x1, 1.0)), w)


def b_tri(p):
    x0, y0, xa, ya, x1, y1, w = p
    return svg_stroke(W17, H17, d_poly([(x0, y0), (xa, ya), (x1, y1)]), w)


def b_saw(p):
    x0, y0, xv, ytop, ybot, w = p
    return svg_stroke(W17, H17, d_poly([(x0, y0), (xv, ytop), (xv, ybot)]), w)


def b_rsaw(p):
    xv, ybot, ytop, x1, y1, w = p
    return svg_stroke(W17, H17, d_poly([(xv, ybot), (xv, ytop), (x1, y1)]), w)


def b_square(p):
    x0, ytop, xm, ybot, x1, w = p
    return svg_stroke(W17, H17,
                      d_poly([(x0, ytop), (xm, ytop), (xm, ybot), (x1, ybot)]), w)


def b_exp(p):
    """A bell whose sides are exp(-(d/s)^q): q near 1.2 gives the pointed top
    and the long flat skirts the artwork has, which a gaussian cannot."""
    x0, x1, xc, ybase, A, s, q, w = p
    f = lambda x: ybase - A * math.exp(-(abs(x - xc) / s) ** q)

    def df(x):
        u = abs(x - xc) / s
        if u < 1e-9:
            return 0.0
        return (A * q / s) * u ** (q - 1) * math.exp(-u ** q) * (1 if x > xc else -1)

    left = fit_knots(f, df, xc, x0, 2.0)               # bunched at the apex
    segs = [tuple(reversed(s)) for s in reversed(left)] + fit_knots(f, df, xc, x1, 2.0)
    return svg_stroke(W17, H17, d_segs(segs), w)


def b_hold(p):
    xa, xb, xc, xd, y0, y1, y2, y3, w = p
    return svg_stroke(W17, H17, d_poly([(xa, y0), (xa, y1), (xb, y1), (xb, y2),
                                        (xc, y2), (xc, y3), (xd, y3)]), w)


def b_slide(p):
    pts = [(p[i], p[i + 1]) for i in range(0, len(p) - 1, 2)]
    return svg_stroke(W17, H17, d_poly(pts), p[-1])


def dirac_pts(p, flip=False):
    x0, xl, xa, ya, xr, x1, yb, w = p
    pts = [(x0, yb), (xl, yb), (xa, ya), (xr, yb), (x1, yb)]
    if flip:
        pts = [(x, H17 - y) for x, y in pts]
    return pts


def b_dirac(p):
    return svg_stroke(W17, H17, d_poly(dirac_pts(p)), p[7])


def b_dirac_flip(p):
    return svg_stroke(W17, H17, d_poly(dirac_pts(p, True)), p[7])


# 51 is a scatter of dots rather than a curve. These are the centroids of the
# PNG's ten connected components, which the fit then nudges; circles beat
# squares of the same area by better than two to one on the rendered delta.
DOTS = [(4.71, 4.15), (12.39, 5.00), (19.71, 4.62), (6.98, 6.73), (16.02, 7.50),
        (10.47, 9.61), (3.35, 10.12), (13.63, 12.34), (17.84, 11.67), (8.00, 12.73)]


def d_disc(cx, cy, r):
    f = fmt
    return ("M%s %sA%s %s 0 1 0 %s %sA%s %s 0 1 0 %s %sZ"
            % (f(cx - r), f(cy), f(r), f(r), f(cx + r), f(cy),
               f(r), f(r), f(cx - r), f(cy)))


def b_dots(p):
    """One radius for all ten; each centre is then free to move."""
    r, rest = p[0], p[1:]
    d = "".join(d_disc(rest[2 * i], rest[2 * i + 1], r) for i in range(len(rest) // 2))
    return ('<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d">\n'
            '<path d="%s" fill="%s"/>\n</svg>\n' % (W17, H17, W17, H17, d, STROKE))


# ------------------------------------------------------------- pill shapes --
def pill2(x0, x1, y0, y1, rx, ry):
    """A rounded rect whose corners may be elliptical, so that a glow can be
    offset further vertically than horizontally and stay the same shape."""
    f = fmt
    return ("M%s %sH%sA%s %s 0 0 1 %s %sV%sA%s %s 0 0 1 %s %sH%sA%s %s 0 0 1 %s %sV%sA%s %s 0 0 1 %s %sZ"
            % (f(x0 + rx), f(y0), f(x1 - rx), f(rx), f(ry), f(x1), f(y0 + ry),
               f(y1 - ry), f(rx), f(ry), f(x1 - rx), f(y1), f(x0 + rx),
               f(rx), f(ry), f(x0), f(y1 - ry), f(y0 + ry), f(rx), f(ry),
               f(x0 + rx), f(y0)))


def stadium(box, ex=0.0, ey=None):
    """The LED body, expanded by ex horizontally and ey vertically."""
    x0, x1, y0, y1 = box
    ey = ex if ey is None else ey
    r = (y1 - y0) / 2
    return pill2(x0 - ex, x1 + ex, y0 - ey, y1 + ey, r + ex, r + ey)


W25, H25 = 25, 14
GLOW_T = [7.0, 6.5, 6.0, 5.5, 5.0, 4.5, 4.0, 3.5, 3.0, 2.5, 2.0, 1.5, 1.0, .5]
HALO_T = [1.75, 1.25, .75, .25]


def led_svg(box, rho, glow, glow_rgb, body, rim_rgb, rim, grad=None, halo=()):
    L = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d">'
         % (W25, H25, W25, H25)]
    if grad:
        gx0, gx1, stops = grad
        L.append('<defs><linearGradient id="g" gradientUnits="userSpaceOnUse" '
                 'x1="%s" y1="0" x2="%s" y2="0">%s</linearGradient></defs>'
                 % (fmt(gx0), fmt(gx1),
                    "".join('<stop offset="%s" stop-color="%s"/>' % (fmt(o, 3), c)
                            for o, c in stops)))
    for t, o in zip(GLOW_T, glow):
        if o < 0.002:
            continue
        L.append('<path d="%s" fill="%s" fill-opacity="%s"/>'
                 % (stadium(box, t, t * rho), glow_rgb, fmt(o, 3)))
    for t, o in zip(HALO_T, halo):                 # a white bevel under the glow
        if o < 0.002:
            continue
        L.append('<path d="%s" fill="#fff" fill-opacity="%s"/>'
                 % (stadium(box, t, t * rho), fmt(o, 3)))
    L.append('<path d="%s" fill="%s"/>' % (stadium(box), body))
    L.append('<path d="%s%s" fill="%s" fill-rule="evenodd"/>'
             % (stadium(box), inner(box, rim), rim_rgb))
    L.append("</svg>")
    return "\n".join(L) + "\n"


def inner(box, rim):
    """The lit part of the LED: the pill inset by the dark rim, which is not
    the same thickness top and bottom in either state."""
    ix, it, ib = rim
    x0, x1, y0, y1 = box
    x0, x1, y0, y1 = x0 + ix, x1 - ix, y0 + it, y1 - ib
    ry = (y1 - y0) / 2
    return pill2(x0, x1, y0, y1, min(ry, (x1 - x0) / 2), ry)


# The two LEDs share a pill and differ in what fills it: 42's body and glow are
# one blue and its inner rim one dark blue-grey, 41's body is a horizontal ramp
# from near black to a pale blue under a white glow. Every colour is read off
# fully opaque pixels of the PNG rather than picked.
@lru_cache(maxsize=None)
def pixel(number, x, y):
    a = target(number)
    assert a[y, x, 3] == 1.0, "colour read from a partly covered pixel"
    return hexc(tuple(int(round(v * 255)) for v in a[y, x, :3]))


@lru_cache(maxsize=None)
def row_colours(number, y0, y1, xs):
    """Mean colour of the fully opaque rows y0..y1 at each x, as gradient stops."""
    a = target(number)
    out = []
    for x in xs:
        band = a[y0:y1 + 1, x]
        assert (band[:, 3] == 1.0).all()
        out.append((x + .5, hexc(tuple(int(round(v * 255)) for v in band[:, :3].mean(axis=0)))))
    return out


def b_led_on(p):
    box, rho, rim = list(p[:4]), p[4], p[5:8]
    glow, halo = p[8:8 + len(GLOW_T)], p[8 + len(GLOW_T):]
    return led_svg(box, rho, glow, pixel(42, 12, 7), pixel(42, 12, 7),
                   pixel(42, 7, 7), rim, halo=halo)


def b_led_off(p):
    box, rho, rim, glow = list(p[:4]), p[4], p[5:8], p[8:]
    gx0, gx1 = box[0] + rim[0], box[1] - rim[0]
    cols = row_colours(41, 7, 8, (8, 10, 12, 14, 16))
    stops = [((x - gx0) / (gx1 - gx0), c) for x, c in cols]
    return led_svg(box, rho, glow, "#fff", "url(#g)", pixel(41, 7, 7), rim,
                   grad=(gx0, gx1, stops))


# ------------------------------------------------------------------- thumb --
W4, H4 = 4, 8


@lru_cache(maxsize=None)
def thumb_stops():
    """The thumb's colour ramp, one stop per fully opaque row. The two interior
    columns differ by a few units; the mean of them is the ramp."""
    a = target(40)
    out = []
    for y in range(a.shape[0]):
        sel = a[y, :, 3] > .85
        if sel.sum() >= 2:
            out.append((y + .5, hexc(tuple(int(round(v * 255))
                                           for v in a[y, sel, :3].mean(axis=0)))))
    return out


def b_thumb(p):
    """The measured vertical ramp in a pill, with a dark border that is heavy
    on the left and fades out to the right: the thumb is lit from that side.
    The border is black at an opacity rather than a second colour, so it follows
    the ramp down instead of fighting it."""
    x0, x1, y0, y1, rx, ry, inset, shl, shr = p
    cols = thumb_stops()
    ya, yb = cols[0][0], cols[-1][0]
    defs = ('<linearGradient id="g" gradientUnits="userSpaceOnUse" x1="0" y1="%s" x2="0" y2="%s">%s</linearGradient>'
            % (fmt(ya), fmt(yb),
               "".join('<stop offset="%s" stop-color="%s"/>' % (fmt((y - ya) / (yb - ya), 3), c)
                       for y, c in cols)))
    border = max(shl, shr) > .002 and inset > .002
    if border:
        defs += ('<linearGradient id="s" gradientUnits="userSpaceOnUse" x1="%s" y1="0" x2="%s" y2="0">'
                 '<stop offset="0" stop-color="#000" stop-opacity="%s"/>'
                 '<stop offset="1" stop-color="#000" stop-opacity="%s"/></linearGradient>'
                 % (fmt(x0), fmt(x1), fmt(shl, 3), fmt(shr, 3)))
    L = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d">'
         % (W4, H4, W4, H4),
         '<defs>%s</defs>' % defs,
         '<path d="%s" fill="url(#g)"/>' % pill2(x0, x1, y0, y1, rx, ry)]
    if border:
        L.append('<path d="%s%s" fill="url(#s)" fill-rule="evenodd"/>'
                 % (pill2(x0, x1, y0, y1, rx, ry),
                    pill2(x0 + inset, x1 - inset, y0 + inset, y1 - inset,
                          max(.05, rx - inset), max(.05, ry - inset))))
    L.append('</svg>')
    return "\n".join(L) + "\n"


# One pen for the whole family. Fitting the width per icon lands between 1.68
# and 1.85 -- the spread is the 4x grid the originals were snapped to, not
# eleven different pens -- and holding it at 1.75, the width most of the
# straight segments actually measure, costs about 0.4/255 an icon.
PEN = 1.75

ICONS = [
    (43, "LFOSine", b_sine,
     [7.5, 4.05, 9.0, 9.0, .72, 2.2, 21.0, PEN],
     [.3, .3, .3, .3, .06, .3, .3, 0]),
    (44, "LFOTriangle", b_tri,
     [2.3, 13.4, 11.6, 2.6, 20.3, 13.4, PEN],
     [.3] * 6 + [0]),
    (45, "LFOSawtooth", b_saw,
     [2.3, 13.4, 20.9, 4.0, 14.2, PEN],
     [.3] * 5 + [0]),
    (46, "LFOReverseSaw", b_rsaw,
     [3.1, 14.2, 3.9, 21.2, 13.6, PEN],
     [.3] * 5 + [0]),
    (47, "LFOSquare", b_square,
     [2.4, 4.6, 12.1, 13.25, 20.9, PEN],
     [.3] * 5 + [0]),
    (48, "LFOExponent", b_exp,
     [2.2, 20.9, 12.0, 12.95, 9.58, 2.70, 1.21, PEN],
     [.3, .3, .2, .2, .3, .3, .1, 0]),
    (49, "LFORandomHold", b_hold,
     [2.875, 9.75, 17.1, 21.8, 10.94, 3.75, 13.25, 6.5, PEN],
     [.3] * 8 + [0]),
    (50, "LFORandomSlide", b_slide,
     [2.2, 8.5, 6.3, 3.4, 11.3, 13.6, 16.9, 6.3, 21.3, 12.2, PEN],
     [.3] * 10 + [0]),
    (51, "LFORandomWhacko", b_dots,
     [1.37] + [v for c in DOTS for v in c],
     [.08] + [.25] * 20),
    (52, "LFODirac", b_dirac,
     [2.2, 6.0, 8.87, 5.18, 12.08, 20.9, 13.25, PEN],
     [.3] * 7 + [0]),
]


def ring_opacities(cumulative):
    """Opacities for rings painted outside in, from the alpha each is meant to
    reach: a ring only has to make up the difference over the one before it."""
    out, sofar = [], 0.0
    for a in cumulative:
        o = (a - sofar) / max(1e-6, 1 - sofar)
        out.append(max(0.0, o))
        sofar = a
    return out


# 42's glow, read off the middle of its long edge and extrapolated to the pill,
# and 41's, which is fainter and tighter. Both are only a starting point: the
# ring opacities are fitted.
ON_GLOW0 = [.31 * math.exp(-.4 * t) for t in GLOW_T]
OFF_GLOW0 = [a * .45 for a in ON_GLOW0]
LED_BOX = [7.0, 18.0, 5.41, 10.46]


def fit_led(number, build, glow0, tag, box=None, halo0=()):
    """The glow and the pill are fitted in turn: they only meet at the rim, and
    chasing both at once just walks one into the other."""
    want = target(number)
    p = (box or LED_BOX) + [1.3, .95, .88, 1.03] + ring_opacities(glow0) + list(halo0)
    n = len(p) - 8

    def steps(geom, rho, ring):
        return [0 if box else geom] * 4 + [rho] + [geom] * 3 + [ring] * n

    for rho, ring, geom in ((.15, .01, 0), (0, 0, .2), (.07, .006, 0), (0, 0, .1),
                            (.03, .004, 0)):
        p, err = fit(p, steps(geom, rho, ring), build, want, tag)
    return p, err


def write(number, svg):
    H, W = target(number).shape[:2]
    assert 'width="%d" height="%d" viewBox="0 0 %d %d"' % (W, H, W, H) in svg, \
        "the canvas has to stay the PNG's size: widgets lay out from it"
    path = os.path.join(SKIN, "%02d.svg" % number)
    with open(path, "w") as fh:
        fh.write(svg)
    return len(svg)


def report(number, name, svg, err):
    n = write(number, svg)
    print("%-16s %02d  %2dx%-2d  delta=%5.2f/255  %5d bytes"
          % (name, number, target(number).shape[1], target(number).shape[0], err, n))


def main():
    dirac = None
    for number, name, build, p0, steps in ICONS:
        want = target(number)
        p, err = fit(p0, steps, build, want, "%02d" % number)
        report(number, name, build(p), err)
        if number == 52:
            dirac = p
        if number == 43:
            print("      handles %.0f%% of the half period, against 36.4%% for a "
                  "true sine" % (p[4] * 100))
            _, e = fit([7.5, 4.05, 9.0, 9.0, 2.2, 21.0, PEN], [.3] * 6 + [0],
                       b_truesine, want, "43t")
            print("      the same icon drawn as an exact sine: %.2f/255" % e)
        if number == 48:
            print("      bell exp(-(d/%.2f)^%.2f), %.2f tall on a baseline at %.2f"
                  % (p[5], p[6], p[4], p[3]))
        if number == 51:
            print("      ten dots of radius %.2f" % p[0])

    # 53 is 52 upside down, pixel for pixel, so it is the same path mirrored.
    svg = b_dirac_flip(dirac)
    report(53, "LFOdIRAC", svg, delta(render(svg, W17, H17, "53"), target(53)))

    def round_pill(q):
        return (0 < q[4] <= (q[1] - q[0]) / 2 + 1e-9
                and 0 < q[5] <= (q[3] - q[2]) / 2 + 1e-9
                and 0 <= q[6] < (q[1] - q[0]) / 2 and all(0 <= v <= 1 for v in q[7:]))

    p, err = fit([.48, 3.45, .64, 7.3, 1.40, 3.17, .3, .9, .2], [.2] * 6 + [.12] * 3,
                 b_thumb, target(40), "40", bound=round_pill)
    report(40, "LFOSliderThumb", b_thumb(p), err)
    print("      box %s  rx=%.2f ry=%.2f  border %.2f at %.0f%%..%.0f%%"
          % (" ".join("%.2f" % v for v in p[:4]), p[4], p[5], p[6],
             p[7] * 100, p[8] * 100))

    # 42 sets the pill; 41 is the same pill with a different glow and filling.
    box = None
    for number, name, build, glow0, halo0 in ((42, "LEDOn", b_led_on, ON_GLOW0,
                                               [.03, .05, .07, .09]),
                                              (41, "LEDOff", b_led_off, OFF_GLOW0, [])):
        p, err = fit_led(number, build, glow0, "%02d" % number, box, halo0)
        box = list(p[:4])
        report(number, name, build(p), err)
        print("      pill %s  rho=%.2f rim x%.2f t%.2f b%.2f" %
              (" ".join("%.2f" % v for v in p[:4]), p[4], p[5], p[6], p[7]))


if __name__ == "__main__":
    main()
