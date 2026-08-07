#!/usr/bin/env python3
"""assets/skin/16.svg -- the eject button.

Deliberately an approximation rather than a fit, and the only file here that is.
The bitmap is hand drawn and not self-consistent: its rim is full alpha down the
right edge and 218 down the left, the bottom runs two rows where the sides run
one, and the fill fades in over four rows at the top against one at the sides.
Fitting that faithfully was tried and does not get past 16/255, because what is
left is per-edge fudge rather than model error -- and reproducing a wobble
exactly buys nothing once the artwork is a vector being drawn at 150 %, which is
where the bitmap looked worst.

So this draws the icon the artwork was trying to be: a bowl with a sharp blue
rim, a flat interior and a blue cross. No gradient and no soft halo -- flat
paths, by request, because at this size a sharp edge reads better than a
faithful blur. The silhouette is still fitted, because that is the part a wrong
guess shows in.
"""
import os
import subprocess
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from svgify import SKIN, TMP, descend, fmt  # noqa: E402

RIM = "#13b5ea"
INTERIOR = "#6f777b"  # flat, the area-weighted mean of the bitmap's grey ramp

RIM_WIDTH = 1.25
SS = 8  # supersampling for the silhouette fit

# Where the body actually starts. The fit puts the silhouette's top at ~4.0,
# but the bitmap is only ~65 % opaque there and does not reach full until ~6.
SOLID_TOP = 5.40
FADE_TOP = 4.15
FADE_OPACITY = 0.55

CROSS = dict(cx=15.50, cy=9.70, half_w=2.85, half_h=3.20, width=2.00)


def coverage(p, shape):
    """Supersampled coverage of the bowl described by `p`.

    The bowl is a rectangle whose bottom is a superellipse. The exponent is
    fitted rather than assumed: an ellipse (n=2) comes out visibly too wide
    across the bottom and a parabola too narrow.
    """
    left, right, top, straight, depth, n = p
    if right - left <= 1 or depth <= 0.5 or n <= 0.2 or straight <= top:
        return None
    H, W = shape
    ys, xs = np.mgrid[0:H * SS, 0:W * SS]
    px, py = (xs + .5) / SS, (ys + .5) / SS
    cx, hw = (left + right) / 2, (right - left) / 2
    u = np.abs((px - cx) / hw)
    v = np.clip((py - straight) / depth, 0, None)
    inside = (px >= left) & (px <= right) & (py >= top) & (py <= straight + depth)
    inside &= (py <= straight) | (np.power(u, n) + np.power(v, n) <= 1.0)
    return inside.astype(float).reshape(H, SS, W, SS).mean(axis=(1, 3))


def fit_bowl(target):
    def cost(p):
        c = coverage(p, target.shape)
        return 1e9 if c is None else float(np.abs(c - target).mean())

    return descend([3.6, 27.4, 3.6, 11.0, 5.0, 2.0], cost, [.4, .4, .4, .5, .5, .4])


def bowl_path(p, inset: float = 0.0, top_override=None) -> str:
    """The fitted bowl as a path, shrunk by `inset` on the sides and bottom.

    The top edge is not inset: the artwork has no rim across it, so the interior
    reaches the same y as the rim and the two share that edge.
    """
    left, right, top, straight, depth, n = p
    if top_override is not None:
        top = top_override
    left += inset
    right -= inset
    depth -= inset
    cx, hw = (left + right) / 2, (right - left) / 2
    f = fmt
    out = ["M%s %sV%s" % (f(left), f(top), f(straight))]
    for i in range(1, 16):                      # the superellipse, sampled
        ang = (i / 16.0) * np.pi
        u, v = -np.cos(ang), np.sin(ang)
        out.append("L%s %s" % (f(cx + hw * np.sign(u) * abs(u) ** (2.0 / n)),
                               f(straight + depth * v ** (2.0 / n))))
    out.append("L%s %sV%sZ" % (f(right), f(straight), f(top)))
    return "".join(out)


def cross_path() -> str:
    c, f = CROSS, fmt
    x0, x1 = c["cx"] - c["half_w"], c["cx"] + c["half_w"]
    y0, y1 = c["cy"] - c["half_h"], c["cy"] + c["half_h"]
    return "M%s %sL%s %sM%s %sL%s %s" % (f(x0), f(y0), f(x1), f(y1),
                                         f(x1), f(y0), f(x0), f(y1))


def main() -> None:
    png = os.path.join(SKIN, "16.png")
    ref = Image.open(png).convert("RGBA")
    alpha = np.asarray(ref, dtype=np.float64)[:, :, 3] / 255.0
    # The halo is soft and wide; the body is what has an edge. 0.6 sits above
    # the halo everywhere it appears.
    p, err = fit_bowl((alpha > 0.60).astype(float))
    print("bowl  left=%.2f right=%.2f top=%.2f straight=%.2f depth=%.2f n=%.2f  (err %.4f)"
          % (*p, err))

    ############################################################################
    #
    #   The bitmap does not start at its top edge: it fades in over about two
    # rows, so the icon reads as sitting below the top of its canvas with a gap
    # above. Drawn with a hard edge at the fitted top it comes out looking
    # taller and heavier than the original at the same nominal size.
    #
    #   So the solid body starts at SOLID_TOP and one half-opacity copy above it
    # stands in for the fade. Two paths rather than a gradient, which is both
    # what was asked for and what keeps the edge sharp where it matters.
    #
    ############################################################################
    faded = "".join([
        '<path d="%s" fill="%s"/>' % (bowl_path(p, top_override=FADE_TOP), RIM),
        '<path d="%s" fill="%s"/>' % (bowl_path(p, RIM_WIDTH, top_override=FADE_TOP), INTERIOR),
    ])

    svg = "\n".join([
        '<svg xmlns="http://www.w3.org/2000/svg" width="30" height="18" viewBox="0 0 30 18">',
        '<g opacity="%s">%s</g>' % (fmt(FADE_OPACITY), faded),
        '<path d="%s" fill="%s"/>' % (bowl_path(p, top_override=SOLID_TOP), RIM),
        '<path d="%s" fill="%s"/>' % (bowl_path(p, RIM_WIDTH, top_override=SOLID_TOP), INTERIOR),
        '<path d="%s" fill="none" stroke="%s" stroke-width="%s" stroke-linecap="round"/>'
        % (cross_path(), RIM, fmt(CROSS["width"])),
        '</svg>',
    ]) + "\n"

    out_svg = os.path.join(SKIN, "16.svg")
    with open(out_svg, "w") as fh:
        fh.write(svg)

    out = os.path.join(TMP, "eject.png")
    subprocess.run(["rsvg-convert", "-w", str(ref.width), "-h", str(ref.height),
                    out_svg, "-o", out], check=True)
    print("16.svg %d bytes (png %d)" % (len(svg), os.path.getsize(png)))

    z = 12
    sheet = Image.new("RGB", (ref.width * z * 2 + 8, ref.height * z), (40, 40, 46))
    sheet.paste(ref.convert("RGB").resize((ref.width * z, ref.height * z), Image.NEAREST), (0, 0))
    sheet.paste(Image.open(out).convert("RGB").resize((ref.width * z, ref.height * z),
                                                      Image.NEAREST), (ref.width * z + 8, 0))
    sheet.save(os.path.join(TMP, "eject_pair.png"))


if __name__ == "__main__":
    main()
