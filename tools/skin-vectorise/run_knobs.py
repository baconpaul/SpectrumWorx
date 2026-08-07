#!/usr/bin/env python3
"""Vectorise the SpectrumWorx skin's knob film strips.

`02`, `03`, `12`, `63` and `64` are film strips: 127 square frames stacked
vertically, picked by `Knob::paintFilmStrip` as `126 * proportion`, so frame f
has to sit at y = f * width or every knob in the editor points somewhere else.
The layout is kept exactly -- same canvas, same 127 frames -- and only the
drawing becomes vector.

Two families live here, because the artwork turns out to be two things:

  02           the editor knob: a soft bevel shaded off centre, eight fixed
               ticks, a teal ring under a black cap, and a dark bar that
               rotates. Every frame is the same picture at a different angle.
  03/12/63/64  the module knobs: a grey dome and a blue wedge that opens from
               a fixed edge -- 03 and 63 from 7:30 clockwise, 12 and 64 from
               12 o'clock either way -- with a black cap whose radius *is* the
               wedge's inner radius. Nothing rotates: the shape changes.

## Keeping it small

The thing to avoid is spelling the whole drawing out 127 times, so what does
not change is written once into `<defs>` and instanced with `<use>`, and only
the difference is per frame. That is worth about 3.5x against the PNGs.

JUCE's `<use>` is not SVG's, and the differences are all traps that rsvg does
not reproduce:

  - `SVGState::getLinkedID` reads `xlink:href` and only that, so a
    `<use href="#a">` renders perfectly in rsvg and draws *nothing* in the
    plugin. Everything here is `xlink:href`.
  - `parseUsePath` copies the linked element's *path*, so a `<use>` may point
    at a path/circle/rect but never at a `<g>`, and it is styled from the
    `<use>` tag rather than from its target -- the fill goes on the `<use>`
    (or on an ancestor `<g>`, which both renderers inherit from), never on the
    definition, where JUCE would not look for it and rsvg would.
  - `getGradientFillType` never applies the element's transform to a
    `userSpaceOnUse` gradient, so an instance inside a translated `<g>` would
    take its geometry to the frame and leave its gradient at frame zero. Every
    gradient here is therefore in the default `objectBoundingBox` units, which
    JUCE resolves from the *transformed* path's bounds and so follows the
    frame. The cost is that no gradient-filled element may be rotated -- JUCE
    would keep the gradient axis-aligned while rsvg turned it -- and the only
    rotated element is 02's pointer, which is flat.

Each frame is a `<g>` whose translation puts the origin at the knob's centre,
which shortens every number inside it and lets the pointer's rotation be a
bare `rotate(a)`.

## Method

Measured off the PNG, then rendered back through rsvg and compared:

  - 02's static picture is the per-pixel median of the 127 frames: a rotating
    bar is over any one pixel for a frame or two, so the median is what is
    behind it. The others have a frame where the wedge is at its smallest, and
    that is the background everywhere the wedge is not.
  - radial profiles become gradient stops the way 13.svg's do, thinned by
    Douglas-Peucker on premultiplied error.
  - 02's bevel is *not* concentric. Its shading is a radial gradient centred
    2.5px right and 1.1px below the knob, which takes the angular residual in
    that ring from 18/255 to 1.0/255; the rim outside it is concentric, which
    is why they are two layers rather than one.
  - 02's teal ring is a plain vertical ramp -- the same grey added to every
    channel, symmetric left to right -- and fits to 0.3/255.
  - the wedges' inner radius and opening angles are fitted per frame by
    supersampled coverage matching, and 02's bar and sweep by the same against
    the frames it appears in. The sweep comes out at exactly -135 to +135.
"""
import math
import os
import subprocess

import numpy as np
from PIL import Image

from svgify import SKIN, TMP, fmt, hexc, descend

FRAMES = 127
HEAD = ('<svg xmlns="http://www.w3.org/2000/svg" '
        'xmlns:xlink="http://www.w3.org/1999/xlink" '
        'width="%d" height="%d" viewBox="0 0 %d %d">')
SS = 8                    # supersampling for the coverage fits
WEDGE = (19, 183, 234)    # the skin blue, flat across every wedge
CAP = "#000"
POINTER = "#161616"


# --------------------------------------------------------------- loading ----
def strip(number):
    """The film strip as (127, w, w, 4) in 0..255, and the frame width."""
    png = np.asarray(Image.open(os.path.join(SKIN, "%02d.png" % number)).convert("RGBA"),
                     dtype=np.float64)
    w = png.shape[1]
    assert png.shape[0] == w * FRAMES, "%02d is not %d square frames" % (number, FRAMES)
    return png.reshape(FRAMES, w, w, 4), w


def grid(w, ss=1):
    """Offsets from the frame's centre, and radius/bearing (0 = up, clockwise)."""
    ys, xs = np.mgrid[0:w * ss, 0:w * ss]
    x = (xs + .5) / ss - w / 2.
    y = (ys + .5) / ss - w / 2.
    return x, y, np.hypot(x, y), np.degrees(np.arctan2(x, -y))


def shrink(a, ss):
    return a.reshape(a.shape[0] // ss, ss, a.shape[1] // ss, ss).mean(axis=(1, 3))


# ------------------------------------------------------------- gradients ----
def stop(offset, rgb, alpha):
    s = '<stop offset="%s" stop-color="%s"' % (fmt(min(max(offset, 0.), 1.), 4), hexc(rgb))
    if alpha < 0.9995:
        s += ' stop-opacity="%s"' % fmt(alpha, 3)
    return s + "/>"


def simplify(xs, vals, tol, gap=1 / 3.):
    """Douglas-Peucker over a sampled colour+alpha curve, premultiplied error.

    `gap` is the closest two stops may usefully sit: JUCE builds a gradient's
    lookup table at three entries per pixel of its length and rounds each stop
    into one of them, so anything finer is drawn by rsvg and not by the
    plugin."""
    keep = {0, len(xs) - 1}
    while True:
        k = sorted(keep)
        got = np.array([np.interp(xs, [xs[i] for i in k], [vals[i][c] for i in k])
                        for c in range(4)]).T
        d = (np.abs(got[:, :3] * got[:, 3:4] - vals[:, :3] * vals[:, 3:4]).mean(axis=1)
             + np.abs(got[:, 3] - vals[:, 3]))
        d[k] = 0
        if gap:
            for i in k:
                d[np.abs(xs - xs[i]) < gap] = 0
        i = int(np.argmax(d))
        if d[i] < tol:
            return k
        keep.add(i)


def sample_profile(dist, rgba, mask, lo, hi, step=0.25):
    """Straight colour and alpha binned by `dist` over the pixels `mask` picks."""
    mid, vals = [], []
    for a in np.arange(lo, hi, step):
        sel = mask & (dist >= a) & (dist < a + step)
        if sel.sum() < 2:
            continue
        v = rgba[sel] / 255.
        weight = np.maximum(v[:, 3], 0.02)          # colour is read where it exists
        mid.append(a + step / 2)
        vals.append(list((v[:, :3] * weight[:, None]).sum(axis=0) / weight.sum())
                    + [v[:, 3].mean()])
    vals = np.array(vals)
    for i in range(1, len(vals)):                   # past the last visible ring the
        if vals[i, 3] < 0.02:                       # stored colour is noise: hold it
            vals[i, :3] = vals[i - 1, :3]
    return np.array(mid), vals


def stops_for(mid, vals, span, tol=0.005):
    """Stops for a gradient whose parameter runs 0..span."""
    keep = simplify(mid, vals, tol)
    return ("".join(stop(mid[i] / span, tuple(int(round(v * 255)) for v in vals[i][:3]),
                         vals[i][3]) for i in keep),
            len(keep))


# ----------------------------------------------------------------- paths ----
def tight(d):
    """A minus sign already separates two numbers, so the space before it is
    spare -- and both parsers say so: JUCE's parseNextNumber stops at the
    second sign, and the skin's other files are written the same way."""
    return d.replace(" -", "-")


def circle_d(r):
    """A circle about the frame's centre, which every group here is placed at."""
    f = fmt
    return tight("M%s 0A%s %s 0 1 0 %s 0A%s %s 0 1 0 %s 0Z"
                 % (f(-r), f(r), f(r), f(r), f(r), f(r), f(-r)))


def wedge_d(ro, a0, a1):
    """A pie slice from the centre, a0 to a1 clockwise from twelve o'clock."""
    def on(deg):
        t = math.radians(deg)
        return fmt(ro * math.sin(t)), fmt(-ro * math.cos(t))

    x0, y0 = on(a0)
    x1, y1 = on(a1)
    big = 1 if (a1 - a0) % 360 > 180 else 0
    return tight("M0 0L%s %sA%s %s 0 %d 1 %s %sZ" % (x0, y0, fmt(ro), fmt(ro), big, x1, y1))


def bar_d(r0, r1, h0, h1, deg=0.0):
    """A radial bar: r0..r1 out from the centre, half-widths h0 and h1."""
    t = math.radians(deg)
    c, s = math.cos(t), math.sin(t)
    pts = [(x * c - y * s, x * s + y * c) for x, y in
           ((-h0, -r0), (-h1, -r1), (h1, -r1), (h0, -r0))]
    return tight("M" + "L".join("%s %s" % (fmt(x), fmt(y)) for x, y in pts) + "Z")


# --------------------------------------------------------------- compare ----
def write(number, body):
    with open(os.path.join(SKIN, "%02d.svg" % number), "w") as fh:
        fh.write(body)
    return len(body)


def inside(page, *radii):
    """Nothing may reach past its own frame: a frame is a band of the strip and
    ink that overhangs one lands in the next knob along."""
    assert max(radii) <= page + 1e-9, "%s reaches past the frame's %g" % (radii, page)
    return True


def render(svg_text, w, h, tag):
    src = os.path.join(TMP, "k%s.svg" % tag)
    dst = os.path.join(TMP, "k%s.png" % tag)
    with open(src, "w") as fh:
        fh.write(svg_text)
    subprocess.run(["rsvg-convert", "-w", str(w), "-h", str(h), src, "-o", dst], check=True)
    return np.asarray(Image.open(dst).convert("RGBA"), dtype=np.float64)


def delta(got, want):
    """Mean per-pixel error over 255: premultiplied colour plus alpha."""
    gp = got[:, :, :3] * got[:, :, 3:4] / 255.
    wp = want[:, :, :3] * want[:, :, 3:4] / 255.
    return float((np.abs(gp - wp).mean(axis=2) + np.abs(got[:, :, 3] - want[:, :, 3])).mean())


SAMPLED_FRAMES = (0, 1, 32, 63, 95, 126)


def check(number, svg_text, frames, w):
    got = render(svg_text, w, w * FRAMES, "%02d" % number)
    want = frames.reshape(FRAMES * w, w, 4)
    per = [(f, delta(got[f * w:(f + 1) * w], want[f * w:(f + 1) * w])) for f in SAMPLED_FRAMES]
    return delta(got, want), per


# ======================================================= 02 the big knob ====
def half_radius(field, r, mask, lo, hi, step=0.25):
    """Radius at which a radially falling field passes half its inner value."""
    xs, ys = [], []
    for a in np.arange(lo, hi, step):
        sel = mask & (r >= a) & (r < a + step)
        if sel.sum() < 2:
            continue
        xs.append(a + step / 2)
        ys.append(field[sel].mean())
    xs, ys = np.array(xs), np.array(ys)
    half = ys[:4].mean() / 2.
    for i in range(len(xs) - 1):
        if ys[i] >= half > ys[i + 1]:
            return xs[i] + (ys[i] - half) / (ys[i] - ys[i + 1]) * (xs[i + 1] - xs[i])
    return xs[-1]


GAP = .1                       # how far a stop sits from the edge it belongs to
OVERLAP = 1.0                  # how far the rim reaches back under the bevel


def editor_defs(k):
    """The five drawings and the four gradients they are filled with.

    Draw order is bevel, teal, cap, rim, pointer. The rim is one path -- the
    white disc's soft edge, the dark outline around it and the eight ticks --
    because the three sit at radii that do not overlap, so one concentric
    gradient can carry all of them and the frame costs one `<use>` instead of
    three. The outline is a real annulus rather than a bump in a gradient: a
    1px ring rasterises to a different alpha depending on where it falls
    across the pixel grid, and a path edge reproduces that where a gradient,
    sampled once per pixel, cannot.

    The rim's inner edge deliberately overlaps the bevel by a pixel. Two
    antialiased edges meeting exactly leave a seam -- half coverage over half
    coverage composites to three quarters, not to one -- which showed up as a
    45/255 dip in alpha right around the knob."""
    ra, page = k["ra"], k["w"] / 2.
    t0, t1, th = k["tick"]
    rw0, rw1, ri, ro, ringa = k["rim"]
    reach = ra + math.hypot(*k["off"])
    rin, dark = ra - OVERLAP, (0, 0, 0)

    band = (k["rimmid"] >= rin) & (k["rimmid"] <= rw1)
    mid, val = k["rimmid"][band], k["rimval"][band].copy()
    val[:, 3] = np.clip((rw1 - mid) / max(rw1 - rw0, 1e-6), 0, 1)
    keep = simplify(mid, val, .004)
    rims = "".join(
        [stop(o / page, dark, a) for o, a in
         ((0., 0.), (t0 - GAP, 0.), (t0, 1.), (t1, 1.), (t1 + GAP, 0.))]
        + [stop((rin - GAP) / page, tuple(int(round(v * 255)) for v in val[keep[0]][:3]), 1.)]
        + [stop(mid[i] / page, tuple(int(round(v * 255)) for v in val[i][:3]), val[i][3])
           for i in keep]
        + [stop(o / page, dark, a) for o, a in
           ((ri - GAP, 0.), (ri, ringa), (ro, ringa), (min(ro + GAP, page), 0.))])
    bevs, nbev = stops_for(k["bevmid"], k["bevval"], reach)
    caps, ncap = stops_for(k["capmid"], k["capval"], k["rc"], tol=.004)
    return ['<defs>',
            '<radialGradient id="A" cx="%s" cy="%s" r="%s">%s</radialGradient>'
            % (fmt((ra + k["off"][0]) / (2 * ra), 4), fmt((ra + k["off"][1]) / (2 * ra), 4),
               fmt(reach / (2 * ra), 4), bevs),
            '<linearGradient id="B" x1="0" y1="0" x2="0" y2="1">%s%s</linearGradient>'
            % (stop(0., k["top"], 1.), stop(1., k["bot"], 1.)),
            '<radialGradient id="C">%s</radialGradient>' % caps,
            '<radialGradient id="D">%s</radialGradient>' % rims,
            '<path id="a" d="%s"/>' % circle_d(ra),
            '<path id="b" d="%s"/>' % circle_d(k["rb"]),
            '<path id="c" d="%s"/>' % circle_d(k["rc"]),
            '<path id="d" fill-rule="evenodd" d="%s%s%s%s%s"/>'
            % (circle_d(rin), circle_d(rw1), circle_d(ri), circle_d(ro),
               "".join(bar_d(t0, t1, th, th, 45. * n) for n in range(8))),
            '<path id="p" d="%s"/>' % bar_d(*k["point"][:4]),
            '</defs>'], (nbev, 2, ncap, 10 + len(keep))


def editor_frame(k, angle=None):
    """One page, used to fit the rim and the ticks by render-and-compare."""
    w = k["w"]
    defs, _ = editor_defs(k)
    inst = ('<use xlink:href="#a" fill="url(#A)"/><use xlink:href="#b" fill="url(#B)"/>'
            '<use xlink:href="#c" fill="url(#C)"/><use xlink:href="#d" fill="url(#D)"/>')
    if angle is not None:
        inst += '<use xlink:href="#p" fill="%s" transform="rotate(%s)"/>' % (POINTER, fmt(angle, 3))
    return "\n".join([HEAD % (w, w, w, w)] + defs
                     + ['<g transform="translate(%s %s)">%s</g>' % (fmt(w / 2.), fmt(w / 2.), inst),
                        "</svg>"]) + "\n"


def build_editor_knob():
    frames, w = strip(2)
    med = np.median(frames, axis=0)
    x, y, r, th = grid(w)
    page = w / 2.
    ticked = (np.abs(((th + 22.5) % 45) - 22.5) < 8) & (r > 18) & (r < 25.5)

    # --- out to here the disc is opaque everywhere, not just on average
    ra, step = 0., .1
    for lo in np.arange(20., page, step):
        sel = (r >= lo) & (r < lo + step)
        if sel.sum() >= 2 and med[sel][:, 3].min() >= 252.:
            ra = lo + step

    # --- its shading is a radial gradient about a point below and right
    ring = (r >= 19.8) & (r <= ra) & (~ticked)
    grey = med[ring][:, 0]

    def spread(p):
        rho = np.hypot(x - p[0], y - p[1])[ring]
        b = np.floor(rho / .25).astype(int)
        tot, n = 0., 0
        for i in np.unique(b):
            m = b == i
            if m.sum() < 3:
                continue
            tot += np.abs(grey[m] - grey[m].mean()).sum()
            n += m.sum()
        return tot / max(n, 1)

    off, offerr = descend([2.5, 1.], spread, [.5, .5])
    rho = np.hypot(x - off[0], y - off[1])

    # --- teal ring, and the vertical ramp across it
    rb = half_radius(med[:, :, 2] - med[:, :, 0], r, (~ticked) & (r > 16) & (r < 23), 16., 23.)
    body = (r > 15.3) & (r < rb - 1.3) & (~ticked)
    ends, tealerr = [], 0.
    for ch in range(3):
        slope, mid = np.polyfit(y[body], med[body][:, ch], 1)
        ends.append((mid - slope * rb, mid + slope * rb))
        tealerr = max(tealerr, np.abs(med[body][:, ch] - (mid + slope * y[body])).mean())
    top = tuple(int(round(np.clip(e[0], 0, 255))) for e in ends)
    bot = tuple(int(round(np.clip(e[1], 0, 255))) for e in ends)

    # --- black cap: how much of that teal it covers, radius by radius
    tealg = ends[1][0] + (ends[1][1] - ends[1][0]) * (y + rb) / (2 * rb)
    cover = np.clip(1. - med[:, :, 1] / np.maximum(tealg, 1.), 0, 1)
    over = (r > 8.) & (r < rb - 1.3)
    capmid, capval = [0.], [[0., 0., 0., 1.]]
    for lo in np.arange(8., rb - 1.3, .25):
        sel = over & (r >= lo) & (r < lo + .25)
        if sel.sum() < 2:
            continue
        capmid.append(lo + .125)
        capval.append([0., 0., 0., cover[sel].mean()])
    capmid, capval = np.array(capmid), np.array(capval)
    rc = float(capmid[np.argmax(capval[:, 3] < .004)])

    # --- the bevel's colour against distance from that off-centre point, and
    # the rim's against distance from the middle, which it is concentric with
    bevmid, bevval = sample_profile(rho, med, (r >= rb + .3) & (r <= ra) & (~ticked),
                                    14., ra + math.hypot(*off) + .25)
    rimmid, rimval = sample_profile(r, med, np.ones((w, w), bool), ra - OVERLAP - .5, page)

    k = dict(w=w, ra=ra, rb=rb, rc=rc, off=off, top=top, bot=bot,
             rimmid=rimmid, rimval=rimval,
             bevmid=bevmid, bevval=bevval, capmid=capmid, capval=capval,
             tick=(19.1, 22.85, .5), rim=(25.0, 26.1, 26.4, 27.45, .62),
             point=(3.1, 18.5, 1.6, 1.7, -135., 270.))

    # --- ticks and rim, fitted by rendering the page and comparing
    outer = r > 16.5

    def outercost(p):
        t0, t1, thalf, rw0, rw1, ri, ro, ringa = p
        if not (ra + .1 < rw0 < rw1 <= ri < ro <= page and 18. < t0 < t1 < ra
                and .2 < thalf < 1.5 and 0. < ringa <= 1.):
            return 1e9
        k["tick"] = (t0, t1, thalf)
        k["rim"] = (rw0, rw1, ri, ro, ringa)
        got = render(editor_frame(k), w, w, "02f")
        gp = got[:, :, :3] * got[:, :, 3:4] / 255.
        mp = med[:, :, :3] * med[:, :, 3:4] / 255.
        return float((np.abs(gp - mp).mean(axis=2)
                      + np.abs(got[:, :, 3] - med[:, :, 3]))[outer].mean())

    p, outererr = descend([19.1, 22.85, .5, 25.0, 26.1, 26.4, 27.45, .62], outercost,
                          [.25, .25, .12, .25, .25, .25, .12, .06])
    k["tick"] = tuple(p[:3])
    k["rim"] = tuple(p[3:])

    # --- the pointer, read where it lies over the teal and over the cap
    xs, ys, _, _ = grid(w, SS)
    inner, band = (r > .6) & (r < 11.), (r > 16.2) & (r < 18.1)
    seen = {}
    for f in (8, 32, 50, 72, 95, 118):
        o = np.zeros((w, w))
        o[band] = np.clip((tealg[band] - frames[f][:, :, 1][band])
                          / np.maximum(tealg[band] - 22., 1.), 0, 1)
        o[inner] = np.clip(frames[f][:, :, 1][inner] / 22., 0, 1)
        seen[f] = o
    look = inner | band

    def barcov(p, ang):
        r0, r1, h0, h1 = p
        t = math.radians(ang)
        along = xs * math.sin(t) - ys * math.cos(t)
        across = xs * math.cos(t) + ys * math.sin(t)
        hw = h0 + (h1 - h0) * np.clip((along - r0) / max(r1 - r0, 1e-6), 0, 1)
        return shrink(((along >= r0) & (along <= r1) & (np.abs(across) <= hw)).astype(float), SS)

    def pointcost(q):
        if q[0] < 0 or q[1] <= q[0] + 1 or not .2 < q[2] < 4 or not .2 < q[3] < 4:
            return 1e9
        return sum(np.abs(barcov(q[:4], q[4] + q[5] * f / (FRAMES - 1.)) - seen[f])[look].mean()
                   for f in seen) / len(seen)

    point, pointerr = descend([3., 18.9, 1.7, 1.7, -135., 270.], pointcost,
                              [.4, .4, .2, .2, 1., 1.])
    k["point"] = tuple(point)
    inside(page, ra, rb, rc, k["tick"][1], k["rim"][3], point[1])

    defs, stops = editor_defs(k)
    body = ['<g fill="%s">' % POINTER]
    for f in range(FRAMES):
        body.append('<g transform="translate(%s %s)">'
                    '<use xlink:href="#a" fill="url(#A)"/>'
                    '<use xlink:href="#b" fill="url(#B)"/>'
                    '<use xlink:href="#c" fill="url(#C)"/>'
                    '<use xlink:href="#d" fill="url(#D)"/>'
                    '<use xlink:href="#p" transform="rotate(%s)"/></g>'
                    % (fmt(page), fmt(f * w + page),
                       fmt(point[4] + point[5] * f / (FRAMES - 1.), 3)))
    body.append('</g>')
    svg = "\n".join([HEAD % (w, w * FRAMES, w, w * FRAMES)] + defs + body + ["</svg>"]) + "\n"

    k.update(offerr=offerr, tealerr=tealerr, outererr=outererr, pointerr=pointerr, stops=stops)
    return svg, frames, w, k


# ================================================ 03/12/63/64 the wedges ====
def wedge_cover(frame):
    """How much of each pixel the blue covers: it is the only thing here whose
    blue and green differ, so the grey dome and the black cap both read zero
    and the estimate does not care which of them is underneath."""
    return np.clip((frame[:, :, 2] - frame[:, :, 1] - 2.) / (WEDGE[2] - WEDGE[1] - 2.), 0, 1)


def build_wedge_knob(number, quiet_frame):
    frames, w = strip(number)
    x, y, r, th = grid(w)
    page = w / 2.
    xs, ys, rs, ths = grid(w, SS)

    # --- wedge geometry, frame by frame
    def cover(ri, a0, a1, ro):
        return shrink((((rs >= ri) & (rs <= ro)
                        & (((ths - a0) % 360) <= (a1 - a0) % 360)).astype(float)), SS)

    # the outer radius is shared: read it off the two frames with the most edge
    order = np.argsort([-wedge_cover(frames[f]).sum() for f in range(FRAMES)])
    ros = []
    for f in order[:2]:
        seen = wedge_cover(frames[f])

        def rocost(p):
            if p[0] < .2 or p[3] <= p[0] + .5:
                return 1e9
            return np.abs(cover(*p) - seen).mean()

        p, _ = descend([page * .35, -135., 135., page * .72], rocost, [.4, 1.5, 1.5, .4])
        ros.append(p[3])
    ro = float(np.mean(ros))

    geom, err = [], []
    guess = [page * .3, -135., -130.]
    for f in range(FRAMES):
        seen = wedge_cover(frames[f])

        def cost(p):
            if p[0] < .2 or p[0] > ro - .5 or (p[2] - p[1]) % 360 > 350:
                return 1e9
            return np.abs(cover(p[0], p[1], p[2], ro) - seen).mean()

        p, e = descend(list(guess), cost, [.4, 1.5, 1.5])
        guess = list(p)
        geom.append(p)
        err.append(e)
    geom = np.array(geom)

    # --- an edge that does not move is one number, not 127 noisy ones. It
    # matters most where the wedge is a hairline and so says least about its
    # own inner radius: 64's cap is the same size in every frame, and taking
    # the fit at face value shrank it by a third of a pixel at frame 63.
    if geom[:, 0].std() < .25:
        geom[:, 0] = np.median(geom[:, 0])
    # (the symmetric pair's middle frame keeps its own fit: it is the one frame
    # where both edges are at the pivot and neither of them is the pivot's own
    # position -- the wedge there is a hairline with a width of its own)
    for col, lo, hi in ((1, 0, FRAMES), (2, 0, FRAMES), (1, 64, FRAMES), (2, 0, 63)):
        seg = geom[lo:hi, col]
        if seg.std() < .45:
            geom[lo:hi, col] = np.median(seg)
    for col, lo, hi in ((1, 0, 63), (2, 64, FRAMES)):
        seg = geom[lo:hi, col]
        if seg.std() > .45:                       # a moving edge is exactly linear
            k, m = np.polyfit(np.arange(lo, hi), seg, 1)
            geom[lo:hi, col] = k * np.arange(lo, hi) + m

    # --- the dome, from the frame the wedge covers least.
    #
    # The cap is masked out by radius rather than by darkness: the rim is
    # nearly black too, and dropping it for being dark is what leaves the
    # outline grey. How the edge ends is fitted rather than assumed -- the
    # disc's own antialiasing already softens it, so a gradient that fades as
    # well fades it twice, and how much of the fall belongs to which is
    # different at 51px and at 23px.
    quiet = frames[quiet_frame]
    clear = (wedge_cover(quiet) < .02) & (r > geom[quiet_frame][0] + .9)
    reach = math.sqrt(quiet[:, :, 3].sum() / 255. / math.pi)
    look = (r > page * .74) & (r <= page)

    def dome(p):
        solid, edge = p
        mid, val = sample_profile(r, quiet, clear, 0., edge)
        val[:, 3] = np.clip((edge - mid) / max(edge - solid, 1e-6), 0, 1)
        return stops_for(mid, val, edge) + (edge,)

    def domecost(p):
        if not (page * .5 < p[0] <= p[1] <= page):
            return 1e9
        s, _, edge = dome(p)
        one = "\n".join([HEAD % (w, w, w, w),
                         '<defs><radialGradient id="A">%s</radialGradient></defs>' % s,
                         '<path transform="translate(%s %s)" d="%s" fill="url(#A)"/>'
                         % (fmt(page), fmt(page), circle_d(edge)), "</svg>"])
        got = render(one, w, w, "%02dd" % number)
        gp = got[:, :, :3] * got[:, :, 3:4] / 255.
        qp = quiet[:, :, :3] * quiet[:, :, 3:4] / 255.
        return float((np.abs(gp - qp).mean(axis=2)
                      + np.abs(got[:, :, 3] - quiet[:, :, 3]))[look].mean())

    (rsolid, redge), domeerr = descend([reach - .6, reach + .4], domecost, [.2, .2])
    doms, ndom, rdisc = dome([rsolid, redge])
    inside(page, rdisc, ro, geom[:, 0].max())

    defs = ['<defs>',
            '<radialGradient id="A">%s</radialGradient>' % doms,
            '<path id="a" d="%s"/>' % circle_d(rdisc),
            '</defs>']
    body = ['<g fill="%s">' % hexc(WEDGE)]
    for f in range(FRAMES):
        ri, a0, a1 = geom[f]
        body.append('<g transform="translate(%s %s)">'
                    '<use xlink:href="#a" fill="url(#A)"/>'
                    '<path d="%s"/>'
                    '<circle r="%s" fill="%s"/></g>'
                    % (fmt(page), fmt(f * w + page), wedge_d(ro, a0, a1), fmt(ri), CAP))
    body.append('</g>')
    svg = "\n".join([HEAD % (w, w * FRAMES, w, w * FRAMES)] + defs + body + ["</svg>"]) + "\n"
    facts = dict(w=w, ro=ro, rdisc=rdisc, ri=(geom[:, 0].min(), geom[:, 0].max()),
                 a0=(geom[:, 1].min(), geom[:, 1].max()),
                 a1=(geom[:, 2].min(), geom[:, 2].max()),
                 fiterr=float(np.mean(err)), stops=ndom, solid=rsolid, domeerr=domeerr)
    return svg, frames, w, facts


# -------------------------------------------------------------------------
def report(number, svg, frames, w):
    size = write(number, svg)
    whole, per = check(number, svg, frames, w)
    png = os.path.getsize(os.path.join(SKIN, "%02d.png" % number))
    print("    %6d bytes vs %6d png (%.2fx)   mean delta %.2f/255" % (size, png, png / size, whole))
    print("    frames " + "  ".join("%d:%.2f" % (f, d) for f, d in per))
    return size, png, whole


if __name__ == "__main__":
    total_svg = total_png = 0

    svg, frames, w, k = build_editor_knob()
    print("02  editor knob %dx%d" % (w, w * FRAMES))
    print("    bevel r=%.2f shaded about (%+.2f %+.2f) resid %.2f/255 | rim to %.1f"
          % (k["ra"], k["off"][0], k["off"][1], k["offerr"], w / 2.))
    print("    teal r=%.2f  %s->%s resid %.2f/255 | cap r=%.2f | ticks 8x %.2f..%.2f w=%.2f"
          % (k["rb"], k["top"], k["bot"], k["tealerr"], k["rc"],
             k["tick"][0], k["tick"][1], 2 * k["tick"][2]))
    print("    pointer %.2f..%.2f w %.2f..%.2f  sweep %.2f -> %.2f deg (%.2f)"
          % (k["point"][0], k["point"][1], 2 * k["point"][2], 2 * k["point"][3],
             k["point"][4], k["point"][4] + k["point"][5], k["point"][5]))
    print("    stops %s   5 <use> per frame" % (k["stops"],))
    s, p, _ = report(2, svg, frames, w)
    total_svg += s
    total_png += p

    for number, quiet, name in ((3, 0, "module knob"), (12, 63, "symmetric knob"),
                                (63, 0, "small linear knob"), (64, 63, "small symmetric knob")):
        svg, frames, w, k = build_wedge_knob(number, quiet)
        print("%02d  %s %dx%d" % (number, name, w, w * FRAMES))
        print("    dome r=%.2f %d stops | wedge ro=%.2f ri %.2f..%.2f | edges %.2f..%.2f and %.2f..%.2f"
              % (k["rdisc"], k["stops"], k["ro"], k["ri"][0], k["ri"][1],
                 k["a0"][0], k["a0"][1], k["a1"][0], k["a1"][1]))
        print("    dome edge %.2f..%.2f fit %.2f/255 | coverage fit %.4f"
              % (k["solid"], k["rdisc"], k["domeerr"], k["fiterr"]))
        s, p, _ = report(number, svg, frames, w)
        total_svg += s
        total_png += p

    print("total %d bytes of svg for %d of png (%.2fx)"
          % (total_svg, total_png, total_png / float(total_svg)))
