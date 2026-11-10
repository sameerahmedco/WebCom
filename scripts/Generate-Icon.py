"""Generate the multi-size Windows icon from the approved WebCom artwork.

The source image is the WebCom brand asset. This script intentionally does not
redraw or reinterpret it, so Windows, packaging and in-app surfaces stay in sync.
"""
from pathlib import Path
from PIL import Image

root = Path(__file__).resolve().parents[1]
source = root / "resources" / "webcom-icon.png"
target = root / "resources" / "webcom.ico"
image = Image.open(source).convert("RGBA")
side = max(image.size)
canvas = Image.new("RGBA", (side, side), (0, 0, 0, 0))
canvas.alpha_composite(image, ((side-image.width)//2, (side-image.height)//2))
master = canvas.resize((256, 256), Image.Resampling.LANCZOS)
master.save(target, format="ICO", sizes=[
    (16, 16), (20, 20), (24, 24), (32, 32), (40, 40),
    (48, 48), (64, 64), (96, 96), (128, 128), (256, 256),
])
print(f"Generated {target} from {source}")
