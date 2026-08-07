import os, subprocess, sys
from PIL import Image
import os
_HERE = os.path.dirname(os.path.abspath(__file__))
_ROOT = os.path.abspath(os.path.join(_HERE, "..", ".."))
SKIN = os.path.join(_ROOT, "assets", "skin")
TMP = os.environ.get("SKIN_VECTORISE_TMP", os.path.join(_HERE, ".work"))
os.makedirs(TMP, exist_ok=True)




S = 7
nums = [int(v) for v in sys.argv[1].split(",")]
rows = []
for n in nums:
    p = Image.open(os.path.join(SKIN, "%02d.png" % n)).convert("RGBA")
    out = os.path.join(TMP, "p_%02d.png" % n)
    subprocess.run(["rsvg-convert", "-w", str(p.width * S), "-h", str(p.height * S),
                    os.path.join(SKIN, "%02d.svg" % n), "-o", out], check=True)
    rows.append((p.resize((p.width * S, p.height * S), Image.NEAREST),
                 Image.open(out).convert("RGBA")))
W = max(a.width for a, b in rows) * 2 + 30
rh = max(a.height for a, b in rows) + 10
sheet = Image.new("RGBA", (W, rh * len(rows)), (255, 255, 255, 255))
for i, (a, b) in enumerate(rows):
    sheet.alpha_composite(a, (10, i * rh + 5))
    sheet.alpha_composite(b, (20 + a.width, i * rh + 5))
sheet.save(os.path.join(TMP, sys.argv[2]))
print(os.path.join(TMP, sys.argv[2]), sheet.size, "left=PNG right=SVG")
