#!/usr/bin/env python3
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

from svgify import (SKIN, TMP, PILLS, load, fit_geometry, fit_gradient, fit_highlight,
                    fit_text, text_d, emit)


def verify(number):
    svg = os.path.join(SKIN, "%02d.svg" % number)
    out = os.path.join(TMP, "v_%02d.png" % number)
    png = os.path.join(SKIN, "%02d.png" % number)
    t = np.asarray(Image.open(png).convert("RGBA"), dtype=np.float64)
    H, W = t.shape[:2]
    subprocess.run(["rsvg-convert", "-w", str(W), "-h", str(H), svg, "-o", out], check=True)
    r = np.asarray(Image.open(out).convert("RGBA"), dtype=np.float64)
    rp = r[:, :, :3] * r[:, :, 3:4] / 255.0
    tp = t[:, :, :3] * t[:, :, 3:4] / 255.0
    return (np.abs(rp - tp).mean(axis=2) + np.abs(r[:, :, 3] - t[:, :, 3])).mean()


for hi_n, lo_n, word in PILLS:
    lo = load(lo_n)
    hi = load(hi_n)
    H, W = lo.shape[:2]
    box, ge = fit_geometry(lo)
    grad = fit_gradient(lo, box)
    hl, he = fit_highlight(hi, box, grad)
    tp, te = fit_text(lo, box, grad, word, "%02d" % lo_n)
    d, span = text_d(word, tp["cap"], 0.0, tp["baseline"], tp["centre"])

    print("%-9s %dx%d box=[%.2f %.2f %.2f %.2f] r=%.2f  geomErr=%.4f" %
          (word, W, H, box[0], box[2], box[1], box[3], box[4], ge))
    print("          grad %.0f->%.0f @off %.3f   cap=%.2fpx baseline=%.2f  textErr=%.4f" %
          (grad["top"] * 255, grad["bottom"] * 255, grad["offset"], tp["cap"], tp["baseline"], te))
    print("          rim %s d[%.2f,%.2f] rings=%s  hlErr=%.4f" %
          (hl["rim_rgb"], hl["rim_in"], hl["rim_out"],
           ["%.3f" % o for _, o in hl["rings"]], he))

    for number, highlight in ((lo_n, None), (hi_n, hl)):
        s = emit(W, H, box, grad, highlight, d)
        with open(os.path.join(SKIN, "%02d.svg" % number), "w") as fh:
            fh.write(s)
        print("          %02d.svg %5d bytes   render delta vs png = %.2f/255"
              % (number, len(s), verify(number)))
