import sys
from PIL import Image
import os
_HERE = os.path.dirname(os.path.abspath(__file__))
_ROOT = os.path.abspath(os.path.join(_HERE, "..", ".."))
SKIN = os.path.join(_ROOT, "assets", "skin")
TMP = os.environ.get("SKIN_VECTORISE_TMP", os.path.join(_HERE, ".work"))
os.makedirs(TMP, exist_ok=True)


base = SKIN + "/"
for name in sys.argv[1].split(","):
    im = Image.open(base + name + ".png").convert("RGBA")
    px = im.load(); w, h = im.size
    print("=" * 72)
    print("%s  %dx%d" % (name, w, h))
    print("alpha map:")
    for y in range(h):
        print("  %2d %s" % (y, "".join("." if px[x, y][3] == 0 else
                                       ("#" if px[x, y][3] == 255 else "o") for x in range(w))))
    print("corners: TL=%s TR=%s BL=%s BR=%s" % (px[0, 0], px[w-1, 0], px[0, h-1], px[w-1, h-1]))
    print("column x=%d down:" % (w // 2))
    for y in range(h):
        print("   y=%2d %s" % (y, px[w // 2, y]))
    print("row y=%d across (first 10 / last 10):" % (h // 2))
    for x in list(range(0, 10)) + list(range(w - 10, w)):
        print("   x=%2d %s" % (x, px[x, h // 2]))
