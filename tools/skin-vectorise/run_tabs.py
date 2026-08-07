#!/usr/bin/env python3
"""Vectorise the SpectrumWorx skin's settings tabs -- 21/22, 23/24, 27/28.

These are three abutting tabs ("Engine", "GUI", "About"), so unlike the pill
buttons they are not one shape on a transparent page: they full-bleed to the
canvas along the bottom and along whichever side abuts a neighbour.

Each is two stacked shapes. Underneath, a near-black plate, square except at
the tab's one outer top corner (21 top-left, 23 neither, 27 top-right) -- that
is the outline the alpha channel draws. Over it, the body carrying a vertical
gradient, grey when the tab is off and blue when it is on. The body is rounded
on all four corners at r~4.4, which is what leaves the dark notch the artwork
has where two tabs meet, and the plate's outer corner is rounded harder than
the body's, which is why the very corner of the page is body-white rather than
black. Both are measured off the PNG rather than eyeballed; see README.md for
the rules the output has to keep (no filters, canvas = the PNG's size, text as
outlines).
"""
import os
import subprocess

import numpy as np
from PIL import Image
from fontTools.ttLib import TTFont
from fontTools.pens.recordingPen import RecordingPen
from fontTools.pens.boundsPen import BoundsPen

import svgify as S

SKIN = S.SKIN
TMP = S.TMP
SS = S.SS

DARK = np.array([26.0, 23.0, 27.0])          # the plate / seam colour
INK = np.array([35.0, 31.0, 32.0])           # the label, #231f20
TEXT_FILL = "#231f20"
EPS = 0.01

# (off, on, label, plate corners that may round: top-left, top-right)
TABS = [(21, 22, "Engine", (1, 0)),
        (23, 24, "GUI", (0, 0)),
        (27, 28, "About", (0, 1))]


def load(n):
    return np.asarray(Image.open(os.path.join(SKIN, "%02d.png" % n)).convert("RGBA"),
                      dtype=np.float64)


# ------------------------------------------------------------- geometry ----
_grid = {}


def _px(H, W):
    if (H, W) not in _grid:
        ys, xs = np.mgrid[0:H * SS, 0:W * SS]
        _grid[(H, W)] = ((xs + .5) / SS, (ys + .5) / SS)
    return _grid[(H, W)]


def sdf(shape, box):
    """Signed distance to a rect with an independent radius per corner."""
    H, W = shape
    x0, x1, y0, y1, rtl, rtr, rbr, rbl = box
    px, py = _px(H, W)
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    hx, hy = (x1 - x0) / 2, (y1 - y0) / 2
    lim = max(min(hx, hy), 1e-6)
    rtl, rtr, rbr, rbl = (min(max(r, 0.0), lim) for r in (rtl, rtr, rbr, rbl))
    r = np.where(px > cx, np.where(py > cy, rbr, rtr), np.where(py > cy, rbl, rtl))
    qx = np.abs(px - cx) - hx + r
    qy = np.abs(py - cy) - hy + r
    return (np.minimum(np.maximum(qx, qy), 0.0)
            + np.hypot(np.maximum(qx, 0.0), np.maximum(qy, 0.0)) - r)


def cover(shape, box):
    """Per-pixel area coverage of the rect.

    Each supersample contributes the fraction of its own cell that the edge
    leaves inside, not a yes/no -- thresholding quantises coverage to 1/SS,
    which is coarser than the third-of-a-pixel offsets being fitted here and
    left the top edge sitting anywhere in a wide flat basin."""
    return S.down(np.clip(.5 - sdf(shape, box) * SS, 0.0, 1.0))


def band_mask(H, W):
    """Pixels the geometry is fitted on: the rim, kept clear of the label."""
    m = np.zeros((H, W), bool)
    m[:, :4] = True
    m[:, W - 4:] = True
    m[:3, :] = True
    m[H - 3:, :] = True
    return m


# ------------------------------------------------------------- gradient ----
def row_body(png, body):
    """Per-row body colour: the modal colour across the row's interior.

    Modal rather than brightest, because the lit tabs' label is lighter than
    their body in red and darker in blue; and horizontal-only inset, so the
    row just under the top edge -- which the gradient's flat run has to get
    right -- still gets a sample."""
    H, W = png.shape[:2]
    rows, cols = [], []
    xs = np.arange(W) + .5
    inside = (xs > body[0] + 2) & (xs < body[1] - 2)
    for y in range(H):
        if y + .5 <= body[2] or y + .5 >= body[3]:
            continue
        sel = inside & (png[y, :, 3] > 254.5)
        if sel.sum() < 5:
            continue
        vals, counts = np.unique(png[y][sel][:, :3].astype(int), axis=0, return_counts=True)
        if counts.max() < .4 * sel.sum():
            continue
        rows.append(y + .5)
        cols.append(vals[counts.argmax()].astype(float))
    return np.array(rows), np.array(cols)


def stop_pos(K, q):
    """Stop offsets: the first is free, the rest evenly spaced after it.

    A gradient whose line would leave the top of the button brighter than
    white gets a flat run instead, which is what the free first offset buys
    -- the grey body then needs only the two stops svgify's pills use."""
    return q + (1.0 - q) * np.arange(K) / (K - 1.0)


def _basis(t, pos):
    """Piecewise-linear hat functions for stops at the given offsets."""
    K = len(pos)
    A = np.zeros((len(t), K))
    for k in range(K):
        w = np.zeros(len(t))
        if k == 0:
            w += (t <= pos[0]).astype(float)
        else:
            span = max(pos[k] - pos[k - 1], 1e-9)
            w += np.clip((t - pos[k - 1]) / span, 0, 1) * (t <= pos[k])
        if k == K - 1:
            w += (t > pos[k]).astype(float)
        else:
            span = max(pos[k + 1] - pos[k], 1e-9)
            w += np.clip((pos[k + 1] - t) / span, 0, 1) * (t > pos[k])
        A[:, k] = w
    return A


def _boxed_lsq(A, b):
    """min ||A c - b||^2 with 0 <= c <= 255, by cyclic coordinate descent."""
    c = np.zeros((A.shape[1], b.shape[1]))
    den = (A * A).sum(axis=0)
    r = b - A @ c
    for _ in range(400):
        for k in range(A.shape[1]):
            if den[k] < 1e-12:
                continue
            step = (A[:, k] @ (r + np.outer(A[:, k], c[k]))) / den[k]
            new = np.clip(step, 0.0, 255.0)
            r += np.outer(A[:, k], c[k] - new)
            c[k] = new
    return c


def fit_gradient(png, body, y0, y1, kmax=6, tol=1.0):
    rows, cols = row_body(png, body)
    t = np.clip((rows - y0) / (y1 - y0), 0.0, 1.0)
    best = None
    for K in range(2, kmax + 1):
        here = None
        for i in range(41):
            pos = stop_pos(K, i / 200.0)
            A = _basis(t, pos)
            c = _boxed_lsq(A, cols)
            err = np.abs(A @ c - cols).max()
            if here is None or err < here[3]:
                here = (K, pos, c, err)
        best = here
        if here[3] <= tol:
            break
    return dict(K=best[0], pos=best[1], stops=best[2], err=best[3], y0=y0, y1=y1)


def grad_at(grad, y):
    t = np.clip((y - grad["y0"]) / (grad["y1"] - grad["y0"]), 0.0, 1.0)
    A = _basis(t.ravel(), grad["pos"])
    return (A @ grad["stops"]).reshape(y.shape + (3,))


# --------------------------------------------------------------- fitting ---
def render(shape, plate, body, grad):
    """Premultiplied colour (0..255) and alpha of the plate plus the body.

    Compositing happens after downsampling, not before, because that is what
    the rasteriser does: each path is antialiased to a per-pixel coverage and
    then blended, so two edges that overlap in one pixel conflate and the pixel
    ends up more opaque than either.  Along the top of these tabs the plate and
    the body edges land in the same row, and modelling that the other way round
    put the top edge a third of a pixel out.  The gradient is likewise sampled
    once per destination pixel, as cairo and JUCE both do."""
    H, W = shape
    ap = cover(shape, plate)
    ab = cover(shape, body)
    y = np.repeat(np.arange(H, dtype=float)[:, None] + .5, W, axis=1)
    pm = grad_at(grad, y) * ab[:, :, None] + DARK[None, None, :] * (ap * (1 - ab))[:, :, None]
    return pm, ab + ap * (1 - ab)


def fit_shapes(png, grad, rounds):
    """Plate and body together, on premultiplied colour plus alpha.

    Together rather than one then the other: the two share the top edge, so the
    tab's alpha there is neither shape's coverage but the conflation of both.
    Only the outer top corner may take a real radius -- the other three are the
    abutting sides, and get at most the sliver the PNG's corner pixels ask for.

    Edges first, then radii, then both: fourteen parameters at once walks into
    a corner where the plate has pulled a whole pixel off the canvas edge."""
    H, W = png.shape[:2]
    tgt_pm = png[:, :, :3] * png[:, :, 3:4] / 255.0
    tgt_a = png[:, :, 3] / 255.0
    mask = band_mask(H, W)
    rtl_on, rtr_on = rounds
    # x0, x1, y0, plate radii tl/tr/br/bl, body insets l/r/t, body radii
    lo = [0., W - 1.5, 0., 2. if rtl_on else 0., 2. if rtr_on else 0., 0., 0.,
          0., 0., 0., 2., 2., 2., 2.]
    hi = [1.5, float(W), 1.5, 8. if rtl_on else .9, 8. if rtr_on else .9, .9, .9,
          1.5, 1.5, 1.0, 6., 6., 6., 6.]

    def unpack(p):
        plate = [p[0], p[1], p[2], float(H), p[3], p[4], p[5], p[6]]
        body = [p[0] + p[7], p[1] - p[8], p[2] + p[9], float(H)] + list(p[10:])
        return plate, body

    def cost(p):                       # the plate stays inside the canvas: a
        if any(v < lo[i] or v > hi[i] for i, v in enumerate(p)):
            return 1e9                 # path that overhangs moves JUCE's bounds
        plate, body = unpack(p)
        pm, al = render((H, W), plate, body, grad)
        e = np.abs(pm - tgt_pm).sum(axis=2) / 255.0 + 3 * np.abs(al - tgt_a)
        return float(e[mask].mean())

    p = [.75, float(W), .75, 5. if rtl_on else .2, 5. if rtr_on else .2, .2, .2,
         .25, .5, .0, 4.5, 4.5, 4.5, 4.5]
    edges = [.3, .3, .2, 0, 0, 0, 0, .2, .2, .15, 0, 0, 0, 0]
    radii = [0, 0, 0, .8, .8, .3, .3, 0, 0, 0, .8, .8, .8, .8]
    both = [.2, .2, .15, .6, .6, .3, .3, .15, .15, .1, .6, .6, .6, .6]
    for step in (edges, radii, both, both):
        p, err = S.descend(p, cost, list(step))
    plate, body = unpack(p)
    return plate, body, err


# ------------------------------------------------------------------ text ---
class Face:
    def __init__(self, path):
        self.f = TTFont(path)
        self.upm = self.f["head"].unitsPerEm
        self.cmap = self.f.getBestCmap()
        self.hmtx = self.f["hmtx"]
        self.gs = self.f.getGlyphSet()
        bp = BoundsPen(self.gs)
        self.gs[self.cmap[ord("H")]].draw(bp)
        self.cap = bp.bounds[3]

    def run(self, word):
        recs, advs = [], []
        for ch in word:
            n = self.cmap[ord(ch)]
            p = RecordingPen()
            self.gs[n].draw(p)
            recs.append(p.value)
            advs.append(self.hmtx[n][0])
        return recs, advs


def text_d(face, word, cap_px, tracking, baseline, centre_x):
    """Glyph outlines as one path -- svgify.text_d, but for a chosen face."""
    recs, advs = face.run(word)
    scale = cap_px / (face.cap / face.upm) / face.upm
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
    fmt = S.fmt

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
            r = list(pts) + ([start] if implied else [])
            if len(r) == 1:
                r = [r[0], r[0]]
            for i in range(len(r) - 2):
                c, n = r[i], r[i + 1]
                emit("Q", [fmt(c[0]), fmt(c[1]),
                           fmt((c[0] + n[0]) / 2), fmt((c[1] + n[1]) / 2)])
            emit("Q", [fmt(r[-2][0]), fmt(r[-2][1]), fmt(r[-1][0]), fmt(r[-1][1])])
            cur = r[-1]
            continue
        if op == "curveTo":
            emit("C", [f for q in pts for f in (fmt(q[0]), fmt(q[1]))])
            cur = pts[-1]
    return S.join(toks), (minx + dx, maxx + dx)


def ink_mask(png, body, grad):
    """Per-pixel label coverage and the weight it can be trusted with.

    The gradient runs down to near the label's own colour, so the bottom rows
    carry no contrast to read ink from; those are weighted out rather than
    guessed at (see svgify.ink_mask, which says the same about the pills)."""
    H, W = png.shape[:2]
    d = S.down(sdf(png.shape[:2], body))
    y = np.arange(H, dtype=float)[:, None] + .5
    bg = grad_at(grad, np.repeat(y, W, axis=1))
    contrast = (bg - INK[None, None, :]).sum(axis=2)
    cov = np.clip((bg - png[:, :, :3]).sum(axis=2) / np.maximum(contrast, 1e-6), 0, 1)
    weight = (contrast > 3 * 46.0).astype(float)
    weight[(d > -1.0) | (png[:, :, 3] < 254.5)] = 0
    return cov * weight, weight


def label_cost(face, jobs, cap, base, scratch):
    """Weighted ink error of one (cap, baseline) over every label at once.

    The three tabs are one strip, so they get one size and one seating: fitting
    them separately drifted by a tenth of a pixel each and read as three fonts."""
    tot = 0.0
    for _, word, tgt, wt, centre in jobs:
        H, W = tgt.shape
        d, _ = text_d(face, word, cap, 0.0, base, centre)
        got = S.render_text_only(d, H, W, scratch) * wt
        tot += float(np.abs(got - tgt).sum() / wt.sum())
    return tot


def fit_text(face, jobs, tag):
    """Cap height and baseline, swept rather than descended -- the ink error
    has shallow local minima a coordinate descent walks into."""
    scratch = os.path.join(TMP, "tab_" + tag)
    best = None
    for cap10 in range(50, 81):
        for b10 in range(100, 121):
            cap, base = cap10 / 10.0, b10 / 10.0
            e = label_cost(face, jobs, cap, base, scratch)
            if best is None or e < best[0]:
                best = (e, cap, base)
    err, cap, base = best
    for _ in range(2):                                # refine to a fiftieth
        for d in (.05, .02):
            for i in (0, 1):
                for s in (+d, -d):
                    c, b = (cap + s, base) if i == 0 else (cap, base + s)
                    e = label_cost(face, jobs, c, b, scratch)
                    if e < err:
                        err, cap, base = e, c, b
    return cap, base, err


def ink_mass(face, jobs, cap, base, scratch):
    got, want = 0.0, 0.0
    for _, word, tgt, wt, centre in jobs:
        H, W = tgt.shape
        d, _ = text_d(face, word, cap, 0.0, base, centre)
        got += float((S.render_text_only(d, H, W, scratch) * wt).sum())
        want += float(tgt.sum())
    return got, want


# ------------------------------------------------------------------ emit ---
def rrect(box):
    x0, x1, y0, y1, rtl, rtr, rbr, rbl = box
    lim = max(min((x1 - x0) / 2, (y1 - y0) / 2), 1e-6)
    rtl, rtr, rbr, rbl = (min(max(r, 0.0), lim) for r in (rtl, rtr, rbr, rbl))
    f = S.fmt
    t = ["M", f(x0 + rtl), f(y0), "H", f(x1 - rtr)]
    if rtr > EPS:
        t += ["A", f(rtr), f(rtr), "0", "0", "1", f(x1), f(y0 + rtr)]
    t += ["V", f(y1 - rbr)]
    if rbr > EPS:
        t += ["A", f(rbr), f(rbr), "0", "0", "1", f(x1 - rbr), f(y1)]
    t += ["H", f(x0 + rbl)]
    if rbl > EPS:
        t += ["A", f(rbl), f(rbl), "0", "0", "1", f(x0), f(y1 - rbl)]
    t += ["V", f(y0 + rtl)]
    if rtl > EPS:
        t += ["A", f(rtl), f(rtl), "0", "0", "1", f(x0 + rtl), f(y0)]
    return S.join(t) + "Z"


def emit(W, H, plate, body, grad, text):
    stops = []
    for off, c in zip(grad["pos"], grad["stops"]):
        col = S.hexc(tuple(int(round(v)) for v in np.clip(c, 0, 255)))
        if col == "#ffffff":
            col = "#fff"
        stops.append('<stop offset="%s" stop-color="%s"/>' % (S.fmt(off, 3), col))
    L = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d">'
         % (W, H, W, H),
         '<defs><linearGradient id="g" gradientUnits="userSpaceOnUse" x1="0" y1="%s" x2="0" y2="%s">'
         '%s</linearGradient></defs>' % (S.fmt(grad["y0"]), S.fmt(grad["y1"]), "".join(stops)),
         '<path d="%s" fill="%s"/>' % (rrect(plate), S.hexc(tuple(int(v) for v in DARK))),
         '<path d="%s" fill="url(#g)"/>' % rrect(body),
         '<path d="%s" fill="%s"/>' % (text, TEXT_FILL),
         '</svg>']
    return "\n".join(L) + "\n"


# ---------------------------------------------------------------- compare --
def corners(img):
    H, W = img.shape[:2]
    return [int(round(img[y, x, 3])) for y, x in ((0, 0), (0, W - 1), (H - 1, 0), (H - 1, W - 1))]


def compare(n):
    png = load(n)
    H, W = png.shape[:2]
    out = os.path.join(TMP, "cmp_%02d.png" % n)
    subprocess.run(["rsvg-convert", "-w", str(W), "-h", str(H),
                    os.path.join(SKIN, "%02d.svg" % n), "-o", out], check=True)
    got = np.asarray(Image.open(out).convert("RGBA"), dtype=np.float64)
    a, b = png[:, :, 3:4] / 255.0, got[:, :, 3:4] / 255.0
    e = np.concatenate([np.abs(png[:, :, :3] * a - got[:, :, :3] * b),
                        np.abs(png[:, :, 3:4] - got[:, :, 3:4])], axis=2)
    return float(e.mean()), corners(png), corners(got)


# -------------------------------------------------------------------- run --
def main():
    shapes, jobs = [], []
    for off, on, label, rounds in TABS:
        png = load(off)
        H, W = png.shape[:2]
        assert load(on).shape == png.shape
        body = [.75, W - .75, .75, float(H), 4.5, 4.5, 4.5, 4.5]
        for _ in range(3):
            grad = fit_gradient(png, body, body[2], float(H))
            plate, body, err = fit_shapes(png, grad, rounds)
        tgt, wt = ink_mask(png, body, grad)
        shapes.append((off, on, label, png, plate, body, grad, err))
        jobs.append((off, label, tgt, wt, (body[0] + body[1]) / 2))

    # The face is chosen on how much ink it lays down, not on the per-pixel
    # error: the labels were drawn by a hinted rasteriser that snapped stems to
    # whole pixels, so nothing here lands on the glyph edges exactly and a
    # thinner face wins an L1 contest simply by having less to be wrong about.
    # Mass is the honest reading of weight -- and it agrees with the eye.
    pick = None
    for name in ("Vera.ttf", "VeraBd.ttf"):
        face = Face(os.path.join(SKIN, name))
        cap, base, err = fit_text(face, jobs, name)
        got, want = ink_mass(face, jobs, cap, base, os.path.join(TMP, "tab_" + name))
        off = abs(np.log(got / want))
        print("font %-10s cap=%.2f base=%.2f ink-err=%.4f  ink mass %.1f vs %.1f (%+.0f%%)"
              % (name, cap, base, err, got, want, 100 * (got / want - 1)))
        if pick is None or off < pick[4]:
            pick = (name, face, cap, base, off)
    name, face, cap, base, _ = pick
    print("using %s at cap %.2f, baseline %.2f" % (name, cap, base))

    for (off, on, label, png, plate, body, grad, err), job in zip(shapes, jobs):
        H, W = png.shape[:2]
        d, span = text_d(face, label, cap, 0.0, base, job[4])
        print("%02d/%02d %-6s %dx%d   rim-err=%.4f" % (off, on, label, W, H, err))
        print("   plate x %.3f..%.3f y %.3f..%d  r %s"
              % (plate[0], plate[1], plate[2], H,
                 "/".join("%.2f" % v for v in plate[4:])))
        print("   body  x %.3f..%.3f y %.3f..%d  r %s"
              % (body[0], body[1], body[2], H,
                 "/".join("%.2f" % v for v in body[4:])))
        print("   label ink %.2f..%.2f (%.2f wide)" % (span[0], span[1], span[1] - span[0]))
        with open(os.path.join(SKIN, "%02d.svg" % off), "w") as fh:
            fh.write(emit(W, H, plate, body, grad, d))
        lgrad = fit_gradient(load(on), body, body[2], float(H))
        with open(os.path.join(SKIN, "%02d.svg" % on), "w") as fh:
            fh.write(emit(W, H, plate, body, lgrad, d))
        for n, g in ((off, grad), (on, lgrad)):
            p = os.path.join(SKIN, "%02d.svg" % n)
            delta, cpng, csvg = compare(n)
            print("   %02d.svg %d stops (max %.2f/255)  corners png %s svg %s"
                  "  delta=%.2f/255  %d bytes"
                  % (n, g["K"], g["err"], cpng, csvg, delta, os.path.getsize(p)))


if __name__ == "__main__":
    main()
