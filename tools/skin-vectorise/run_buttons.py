#!/usr/bin/env python3
"""Vectorise the SpectrumWorx skin's module and editor buttons.

These are not the pill buttons `run_pills.py` handles; each one is measured
and then rebuilt from whatever it turns out to be made of:

  04/05  module on / muted -- a 10x4 LED pill with a soft halo, blue when lit
  06     add module        -- a triangle carrying a gradient in colour *and*
                             alpha, from near-black at the base to skin blue
  13/14  trigger off / on  -- one circle, one radial gradient. Everything in
                             the artwork is a function of the radius, the two
                             states differing only inside r < 4.5
  57     change waveform   -- a flat blue triangle
  66/67  user's guide      -- the pill construction again, over two lines

16 (eject) was measured and left as a bitmap. Its cup is not one shape drawn
once: the rim is a pixel of blue on the right at full alpha but only 219 on
the left, the bottom rim runs two rows thick while the sides run one, the fill
fades in over four rows at the top where the sides fade in over one, and it
carries a hand-drawn X. Fitting cup + halo + fill + rim, glyph excluded, does
not get past 16/255 -- worse than the pills manage with the glyph included --
and the residual is all in the rim and the bottom curve, i.e. in the places a
per-edge fudge factor would have to go.

No filters (JUCE 8's SVG parser has none), so a halo is concentric paths whose
opacities are solved from the measured alpha-vs-distance profile rather than
guessed. Stops carry opacity as well as colour, which is only safe because
both renderers interpolate a gradient in straight rather than premultiplied
colour -- juce_ColourGradient.cpp's createLookupTable() tweens
getNonPremultipliedPixelARGB() and premultiplies afterwards, matching the SVG
spec and so rsvg, which is what makes the render-and-compare below mean
anything for a stop whose opacity is 0.
"""
import os
import subprocess
import numpy as np
from PIL import Image

from svgify import (SKIN, TMP, SS, load, fmt, field, down, descend,
                    fit_geometry, fit_gradient, fit_highlight, text_d,
                    pill_path, hexc, RIM_FILL, TEXT_FILL, render_text_only)

HEAD = ('<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" '
        'viewBox="0 0 %d %d">')
STEP_ALPHA = 0.03           # the largest contour a halo shell may leave behind
RING_TIE = 0.10             # how hard the LED's rim is pulled towards its core


# --------------------------------------------------------------- compare ----
def verify(number):
    """Mean per-pixel delta of the rendered SVG against the PNG, over 255.

    Premultiplied colour plus alpha, so a transparent pixel whose stored RGB
    differs cannot invent an error."""
    svg = os.path.join(SKIN, "%02d.svg" % number)
    out = os.path.join(TMP, "v_%02d.png" % number)
    t = np.asarray(Image.open(os.path.join(SKIN, "%02d.png" % number)).convert("RGBA"),
                   dtype=np.float64)
    H, W = t.shape[:2]
    subprocess.run(["rsvg-convert", "-w", str(W), "-h", str(H), svg, "-o", out], check=True)
    r = np.asarray(Image.open(out).convert("RGBA"), dtype=np.float64)
    rp = r[:, :, :3] * r[:, :, 3:4] / 255.0
    tp = t[:, :, :3] * t[:, :, 3:4] / 255.0
    return (np.abs(rp - tp).mean(axis=2) + np.abs(r[:, :, 3] - t[:, :, 3])).mean()


def write(number, body):
    path = os.path.join(SKIN, "%02d.svg" % number)
    with open(path, "w") as fh:
        fh.write(body)
    return len(body)


def stop(offset, rgb, alpha):
    s = '<stop offset="%s" stop-color="%s"' % (fmt(offset, 4), hexc(rgb))
    if alpha < 0.9995:
        s += ' stop-opacity="%s"' % fmt(alpha, 3)
    return s + "/>"


def simplify(xs, vals, tol, gap=0.0):
    """Douglas-Peucker over a sampled colour+alpha curve, premultiplied error.

    `gap` is the closest two stops may sit. JUCE builds a gradient's lookup
    table at three entries per pixel of its length and rounds each stop to one
    of them, so stops nearer than a third of a pixel collapse there however
    faithfully rsvg draws them -- placing them would only make the measured
    delta flatter than what the plugin will show."""
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


# ------------------------------------------------- 13/14 trigger buttons ----
def circle_path(cx, cy, r):
    f = fmt
    return ("M%s %sA%s %s 0 1 0 %s %sA%s %s 0 1 0 %s %sZ"
            % (f(cx - r), f(cy), f(r), f(r), f(cx + r), f(cy),
               f(r), f(r), f(cx - r), f(cy)))


def radial_profile(png, step=0.25):
    """Colour and alpha binned by distance from the centre of the canvas."""
    H, W = png.shape[:2]
    ys, xs = np.mgrid[0:H, 0:W]
    r = np.hypot(xs + .5 - W / 2., ys + .5 - H / 2.)
    R = W / 2.
    edges = np.arange(0, R + step, step)
    mid, vals = [], []
    for lo in edges[:-1]:
        sel = (r >= lo) & (r < lo + step)
        if not sel.any():
            continue
        v = png[sel]
        w = np.maximum(v[:, 3], 0.02)                 # colour is read where it exists
        mid.append(lo + step / 2)
        vals.append(list((v[:, :3] * w[:, None]).sum(axis=0) / w.sum()) + [v[:, 3].mean()])
    mid, vals = np.array(mid), np.array(vals)
    mid[0], mid[-1] = 0.0, R                          # anchor the two ends
    return mid, vals, R


def build_trigger(number, tol=0.008):
    png = load(number)
    H, W = png.shape[:2]
    mid, vals, R = radial_profile(png)
    # past the last visible ring the stored colour is noise: hold it, so that
    # premultiplied and straight interpolation cannot disagree
    last = np.argmax(vals[:, 3] < 0.02)
    if last:
        vals[last:, :3] = vals[last - 1, :3]
    k = simplify(mid, vals, tol, gap=1 / 3.)
    stops = "".join(stop(mid[i] / R, tuple(int(round(v * 255)) for v in vals[i][:3]),
                          vals[i][3]) for i in k)
    L = [HEAD % (W, H, W, H),
         '<defs><radialGradient id="g" gradientUnits="userSpaceOnUse" cx="%s" cy="%s" r="%s">'
         % (fmt(W / 2.), fmt(H / 2.), fmt(R)),
         stops, "</radialGradient></defs>",
         '<path d="%s" fill="url(#g)"/>' % circle_path(W / 2., H / 2., R),
         "</svg>"]
    return "\n".join(L) + "\n", len(k)


# -------------------------------------------------------- 06/57 arrows ----
def tri_cov(shape, p):
    """Supersampled coverage of the triangle (xl,yt) (xr,ym) (xl,yb)."""
    H, W = shape
    xl, yt, yb, xr, ym = p
    ys, xs = np.mgrid[0:H * SS, 0:W * SS]
    px, py = (xs + .5) / SS, (ys + .5) / SS

    def side(ax, ay, bx, by):
        return (bx - ax) * (py - ay) - (by - ay) * (px - ax)

    return down(((px >= xl) & (side(xl, yt, xr, ym) >= 0)
                 & (side(xr, ym, xl, yb) >= 0)).astype(float))


def clip_poly(pts, W, H):
    """Sutherland-Hodgman against the canvas.

    The fitted triangle runs past the page -- the artwork was clipped by its
    own bitmap -- and a drawable is painted at the origin, not scaled to fit,
    so ink outside the page would land outside the widget."""
    for a, b, c, lim in ((1, 0, 0, 0), (-1, 0, 0, -W), (0, 1, 0, 0), (0, -1, 0, -H)):
        def inside(q):
            return a * q[0] + b * q[1] >= lim - 1e-9
        out, n = [], len(pts)
        for i in range(n):
            p, q = pts[i], pts[(i + 1) % n]
            ip, iq = inside(p), inside(q)
            if ip:
                out.append(p)
            if ip != iq:
                dp = a * p[0] + b * p[1] - lim
                dq = a * q[0] + b * q[1] - lim
                t = dp / (dp - dq)
                out.append((p[0] + (q[0] - p[0]) * t, p[1] + (q[1] - p[1]) * t))
        pts = out
        if not pts:
            return pts
    return pts


def tri_path(p, W, H):
    xl, yt, yb, xr, ym = p
    pts = clip_poly([(xl, yt), (xr, ym), (xl, yb)], W, H)
    return "M" + "L".join("%s %s" % (fmt(x), fmt(y)) for x, y in pts) + "Z"


def column_profile(png, minrun=3):
    """The fully covered colour and alpha of each column of the triangle.

    Only columns with a plateau count: near the apex the tallest pixel is
    still a partial one, and taking it at face value bends the ramp."""
    us, vals = [], []
    for x in range(png.shape[1]):
        col = png[:, x]
        m = col[:, 3].max()
        if m < .004:
            continue
        rows = np.nonzero(col[:, 3] >= m - 1.5 / 255)[0]
        if len(rows) < minrun:
            continue
        row = int(rows[len(rows) // 2])
        us.append(x + .5)
        vals.append(list(col[row][:3]) + [col[row][3]])
    us, vals = np.array(us), np.array(vals)
    for _ in range(4):                             # trim any column still short
        f = np.polyfit(us, vals[:, 3], 1)
        keep = np.polyval(f, us) - vals[:, 3] < 1.5 / 255
        if keep.all() or keep.sum() < 4:
            break
        us, vals = us[keep], vals[keep]
    return us, vals


def fit_triangle(png, ramp):
    H, W = png.shape[:2]
    tgt_a = png[:, :, 3]
    tgt_pm = png[:, :, :3] * png[:, :, 3:4]
    ga, gc = ramp

    def cost(p):
        if p[3] <= p[0] or p[2] <= p[1]:
            return 1e9
        cov = tri_cov((H, W), p)
        a = cov * ga
        return (np.abs(gc * a[:, :, None] - tgt_pm).mean(axis=2).mean()
                + np.abs(a - tgt_a).mean())

    ys, xs = np.nonzero(tgt_a > .02)
    base = [xs.min() + .0, ys.min() + .0, ys.max() + 1., xs.max() + 1.,
            (ys.min() + ys.max() + 1) / 2.]
    best = None
    for dl in (0., .5, 1.):                        # the descent is a local one and
        for dr in (0., .5, 1.):                    # these are only a few pixels wide
            p0 = list(base)
            p0[0] += dl
            p0[3] -= dr
            p, e = descend(p0, cost, [.3] * 5)
            if best is None or e < best[1]:
                best = (p, e)
    return best


def build_arrow(number):
    png = load(number)
    H, W = png.shape[:2]
    solid = png[png[:, :, 3] > .5][:, :3]
    flat = solid.size and (solid.max(axis=0) - solid.min(axis=0)).max() < 2.5 / 255
    if flat:                                       # 57 is one flat colour
        rgb = np.round(solid.mean(axis=0) * 255).astype(int)
        ramp = (np.ones((H, W)), np.broadcast_to(rgb / 255., (H, W, 3)))
    else:
        us, vals = column_profile(png)
        fitv = [np.polyfit(us, vals[:, c], 1) for c in range(4)]
        u0 = -fitv[3][1] / fitv[3][0]              # where the ramp reaches alpha 0
        u1 = (1 - fitv[3][1]) / fitv[3][0]         # ... and alpha 1
        ends = [[np.polyval(f, u) for f in fitv] for u in (u0, u1)]
        x = np.arange(W)[None, :] + .5
        t = np.clip((x - u0) / (u1 - u0), 0, 1)
        ga = np.broadcast_to(ends[0][3] + (ends[1][3] - ends[0][3]) * t, (H, W))
        gc = np.stack([ends[0][c] + (ends[1][c] - ends[0][c]) * t for c in range(3)], axis=-1)
        gc = np.broadcast_to(gc, (H, W, 3))
        ramp = (ga, gc)
    p, err = fit_triangle(png, ramp)

    L = [HEAD % (W, H, W, H)]
    if flat:
        fill = hexc(tuple(rgb))
    else:
        L.append('<defs><linearGradient id="g" gradientUnits="userSpaceOnUse" '
                 'x1="%s" y1="0" x2="%s" y2="0">%s%s</linearGradient></defs>'
                 % (fmt(u0, 3), fmt(u1, 3),
                    stop(0, tuple(int(round(np.clip(v, 0, 1) * 255)) for v in ends[0][:3]), 0.0),
                    stop(1, tuple(int(round(np.clip(v, 0, 1) * 255)) for v in ends[1][:3]), 1.0)))
        fill = "url(#g)"
    L.append('<path d="%s" fill="%s"/>' % (tri_path(p, W, H), fill))
    L.append("</svg>")
    return "\n".join(L) + "\n", p, err


# ------------------------------------------------------- 04/05 the LED ----
def led_solve(png, p, glow, want=False):
    """One evaluation of the LED model: halo rings, body, inner body.

    The geometry is what is searched over; everything else falls out of it by
    least squares, which is what keeps the search down to six numbers.

    The halo is a stack of 1px shells, so the alpha it contributes to a pixel
    is a fixed linear combination of the shells' composited alphas -- solve
    those directly. With the halo known the body is linear too: two nested
    pills, each carrying a horizontal ramp, so the unknowns are the four end
    colours. A thin inner inset leaves those two layers nearly collinear, so
    the ring is tied weakly to the core; without that the least squares buys a
    fraction of a pixel of accuracy with a wildly out-of-family rim colour."""
    H, W = png.shape[:2]
    box, ins = list(p[:5]), p[5]
    D = field((H, W), box)
    Cout = down((D <= 0).astype(float))
    Cin = down((D <= -ins).astype(float))
    ta, tpm = png[:, :, 3], png[:, :, :3] * png[:, :, 3:4]
    E = int(np.ceil(np.max(down(D))))
    M = np.array([down(((D > i) & (D <= i + 1)).astype(float)).ravel() for i in range(E)]).T
    A = np.minimum.accumulate(
        np.clip(np.linalg.lstsq(M, (ta - Cout).ravel(), rcond=None)[0], 0, 1))
    ga = (M @ A).reshape(H, W)

    t = np.clip(((np.arange(W)[None, :] + .5) - box[0]) / (box[1] - box[0]), 0, 1) * np.ones((H, 1))
    ring, core = Cout - Cin, Cin
    B = np.stack([(ring * (1 - t)).ravel(), (ring * t).ravel(),
                  (core * (1 - t)).ravel(), (core * t).ravel()], axis=1)
    s = np.sqrt(RING_TIE * np.trace(B.T @ B) / 4)
    Ba = np.vstack([B, s * np.array([[1., 0, -1, 0], [0, 1., 0, -1]])])
    ends = np.array([np.clip(np.linalg.lstsq(
        Ba, np.concatenate([(tpm[:, :, c] - glow[c] * ga).ravel(), [0, 0]]), rcond=None)[0],
        0, 1) for c in range(3)]).T
    pm = np.stack([(B @ ends[:, c]).reshape(H, W) + glow[c] * ga for c in range(3)], axis=-1)
    err = np.abs(pm - tpm).mean(axis=2).mean() + np.abs(Cout + ga - ta).mean()
    return (err, A, ends) if want else err


def build_led(number, glow_rgb):
    png = load(number)
    H, W = png.shape[:2]
    glow = np.array(glow_rgb) / 255.
    ys, xs = np.nonzero(png[:, :, 3] > .999)      # the halo never reaches opaque

    def cost(p):
        if (p[4] < .3 or p[5] < 0 or p[5] > 2
                or min(p[1] - p[0], p[3] - p[2]) < 2 * p[4]):
            return 1e9
        return led_solve(png, p, glow)

    best = None
    for r0 in (1.4, 2.0, 2.6):
        for i0 in (.2, .6, 1.0):
            p, e = descend([xs.min() + .0, xs.max() + 1., ys.min() + .0, ys.max() + 1., r0, i0],
                           cost, [.3] * 5 + [.2])
            if best is None or e < best[1]:
                best = (p, e)
    p, err = best
    _, A, ends = led_solve(png, p, glow, True)
    box, ins = list(p[:5]), p[5]

    L = [HEAD % (W, H, W, H), "<defs>"]
    for i, (a, b) in enumerate(((0, 1), (2, 3))):
        L.append('<linearGradient id="%s" gradientUnits="userSpaceOnUse" x1="%s" y1="0" '
                 'x2="%s" y2="0">%s%s</linearGradient>'
                 % ("ab"[i], fmt(box[0]), fmt(box[1]),
                    stop(0, tuple(int(round(v * 255)) for v in ends[a]), 1.0),
                    stop(1, tuple(int(round(v * 255)) for v in ends[b]), 1.0)))
    L.append("</defs>")
    #   Space the shells by equal steps of alpha rather than equal steps of
    # distance: the profile falls steeply against the pill and flattens out,
    # so 1px shells put every visible contour in the first two of them.
    fine = np.arange(0, len(A) + 1e-9, .05)
    f = np.interp(fine, np.arange(len(A)) + .5, A, left=A[0])
    edges = [0.0]
    for lvl in np.arange(A[0], 0, -STEP_ALPHA):
        d = float(np.interp(-lvl, -f, fine))       # f is decreasing
        if d - edges[-1] >= .3:
            edges.append(d)
    edges.append(float(fine[-1]))
    bands = [(edges[i], edges[i + 1],
              float(f[(fine >= edges[i]) & (fine <= edges[i + 1])].mean()))
             for i in range(len(edges) - 1)]
    rings, below = 0, 0.0
    for lo, hi, lvl in reversed(bands):            # outermost shell first
        o = (lvl - below) / max(1 - below, 1e-6)
        below = lvl
        if o < 0.002:
            continue
        rings += 1
        L.append('<path d="%s" fill="%s" fill-opacity="%s"/>'
                 % (pill_path(box, hi), hexc(tuple(glow_rgb)), fmt(o, 3)))
    L.append('<path d="%s" fill="url(#a)"/>' % pill_path(box))
    L.append('<path d="%s" fill="url(#b)"/>' % pill_path(box, -ins))
    L.append("</svg>")
    return "\n".join(L) + "\n", p, err, rings, ends


# ------------------------------------------------ 66/67 two-line labels ----
def ink_mask(png, box, grad, floor=.10):
    """Per-pixel text coverage and the weight it can be trusted with.

    svgify's version wants 0.18 of contrast between the body gradient and the
    ink; here the second line sits far enough down the gradient that that
    would weigh the whole of it out, and the artwork is clean enough (the ink
    is a flat 35) to read at 0.10."""
    H, W = png.shape[:2]
    d = down(field((H, W), box))
    y = np.arange(H)[:, None] + .5
    x0, x1, y0, y1 = box[:4]
    gy0 = y0 + grad["offset"] * (y1 - y0)
    bg = grad["top"] + (grad["bottom"] - grad["top"]) * np.clip((y - gy0) / max(y1 - gy0, 1e-6), 0, 1)
    contrast = bg - 35 / 255.0
    cov = np.clip((bg - png[:, :, 0]) / np.maximum(contrast, 1e-6), 0, 1)
    weight = np.broadcast_to((contrast > floor).astype(float), cov.shape).copy()
    weight[(d > -1.0) | (png[:, :, 3] < .99)] = 0
    return cov * weight, weight


def fit_two_lines(png, box, grad, words, tag):
    """One cap height, a baseline and a centre for each line, all measured.

    The two lines are not centred on the same axis in the artwork -- 'guide'
    sits about 1.5px left of 'User's' -- so each line's centre is fitted
    rather than assumed."""
    H, W = png.shape[:2]
    target, weight = ink_mask(png, box, grad)
    scratch = os.path.join(TMP, "t2_" + tag)
    wsum = max(weight.sum(), 1.0)

    def paths(p):
        cap = p[0]
        return "".join(text_d(w, cap, 0.0, p[1 + 2 * i], p[2 + 2 * i])[0]
                       for i, w in enumerate(words))

    def cost(p):
        if p[0] < 3 or p[0] > 14:
            return 1e9
        got = render_text_only(paths(p), H, W, scratch) * weight
        return float(np.abs(got - target).sum() / wsum)

    #   Seed each line from its own ink box. The glyph run repeats enough
    # vertical strokes that a centre started on the button's axis walks into a
    # local minimum a whole letter away.
    split = int(round((box[2] + box[3]) / 2))
    p0 = [7.0]
    for a, b in ((int(box[2]), split), (split, int(np.ceil(box[3])))):
        band = target[a:b] > .35
        ys, xs = np.nonzero(band)
        p0 += [a + ys.max() + 1., (xs.min() + xs.max() + 1) / 2.]
    p, err = descend(p0, cost, [.4, .5, .5, .5, .5])
    return p, paths(p), err


def build_guide(lo_n, hi_n, words):
    lo, hi = load(lo_n), load(hi_n)
    H, W = lo.shape[:2]
    box, ge = fit_geometry(lo)
    grad = fit_gradient(lo, box)
    hl, he = fit_highlight(hi, box, grad)
    p, d, te = fit_two_lines(lo, box, grad, words, "%02d" % lo_n)
    out = {}
    for number, glow in ((lo_n, None), (hi_n, hl)):
        L = [HEAD % (W, H, W, H)]
        top = hexc((round(grad["top"] * 255),) * 3)
        L.append('<defs><linearGradient id="g" gradientUnits="userSpaceOnUse" '
                 'x1="0" y1="%s" x2="0" y2="%s">'
                 '<stop offset="%s" stop-color="%s"/><stop offset="1" stop-color="%s"/>'
                 '</linearGradient></defs>'
                 % (fmt(box[2]), fmt(box[3]), fmt(grad["offset"], 3),
                    "#fff" if top == "#ffffff" else top,
                    hexc((round(grad["bottom"] * 255),) * 3)))
        if glow:
            for e, o in glow["rings"]:
                if o >= 0.002:
                    L.append('<path d="%s" fill="#fff" fill-opacity="%s"/>'
                             % (pill_path(box, e), fmt(o, 3)))
        L.append('<path d="%s" fill="url(#g)"/>' % pill_path(box))
        if glow:
            L.append('<path d="%s%s" fill="%s" fill-rule="evenodd"/>'
                     % (pill_path(box, glow["rim_out"]), pill_path(box, glow["rim_in"]), RIM_FILL))
        L.append('<path d="%s" fill="%s"/>' % (d, TEXT_FILL))
        L.append("</svg>")
        out[number] = "\n".join(L) + "\n"
    return out, box, grad, p, (ge, he, te)


if __name__ == "__main__":
    for n in (13, 14):
        s, nstops = build_trigger(n)
        size = write(n, s)
        print("%02d  circle + %d-stop radial gradient   %5d bytes   delta %.2f/255"
              % (n, nstops, size, verify(n)))
    for n in (6, 57):
        s, p, err = build_arrow(n)
        size = write(n, s)
        print("%02d  triangle (%.2f,%.2f) (%.2f,%.2f) (%.2f,%.2f) fitErr %.4f   %5d bytes"
              "   delta %.2f/255"
              % (n, p[0], p[1], p[3], p[4], p[0], p[2], err, size, verify(n)))

    for n, g in ((4, (19, 181, 234)), (5, (255, 255, 255))):
        s, p, err, rings, ends = build_led(n, g)
        size = write(n, s)
        print("%02d  pill [%.2f %.2f %.2f %.2f] r=%.2f inset=%.2f + %d halo shells  fitErr %.4f"
              % (n, p[0], p[2], p[1], p[3], p[4], p[5], rings, err))
        print("    body %s->%s  core %s->%s   %5d bytes   delta %.2f/255"
              % tuple([str(np.round(ends[i] * 255).astype(int)) for i in range(4)]
                      + [size, verify(n)]))

    out, box, grad, p, errs = build_guide(67, 66, (u"User’s", "guide"))
    print("66/67  pill [%.2f %.2f %.2f %.2f] r=%.2f  grad %.0f->%.0f  geomErr %.4f"
          % (box[0], box[2], box[1], box[3], box[4],
             grad["top"] * 255, grad["bottom"] * 255, errs[0]))
    print("       cap=%.2fpx baselines %.2f/%.2f centres %.2f/%.2f  textErr %.4f  hlErr %.4f"
          % (p[0], p[1], p[3], p[2], p[4], errs[2], errs[1]))
    for n in (67, 66):
        size = write(n, out[n])
        print("       %02d.svg %5d bytes   delta %.2f/255" % (n, size, verify(n)))
