import sys, glob, os
from PIL import Image, ImageDraw
d, out = sys.argv[1], sys.argv[2]
tops = sorted(glob.glob(os.path.join(d, "top_*.png")))
cols = 5; W, H = 400, 240 + 240 + 14
rows = (len(tops) + cols - 1) // cols
sheet = Image.new("RGB", (cols * W, rows * H), (40, 40, 40))
for i, t in enumerate(tops):
    x, y = (i % cols) * W, (i // cols) * H
    sheet.paste(Image.open(t).convert("RGB"), (x, y + 14))
    b = t.replace("top_", "bottom_")
    if os.path.exists(b):
        sheet.paste(Image.open(b).convert("RGB"), (x + 40, y + 254))
    ImageDraw.Draw(sheet).text((x + 4, y + 1), os.path.basename(t)[4:10], fill=(255, 255, 0))
sheet.save(out)
