"""Make vr_splash.png Quest-safe: straight alpha (RGB=0 where transparent), minimal PNG chunks."""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np
from PIL import Image

DEFAULT = Path(__file__).resolve().parents[1] / "3OS_Unity_Sample_Project/Assets/StreamingAssets/vr_splash.png"
ALPHA_CUTOFF = 16  # 0-255


def sanitize(path: Path) -> None:
    im = Image.open(path).convert("RGBA")
    arr = np.array(im, copy=True)
    a = arr[..., 3]
    transparent = a < ALPHA_CUTOFF
    arr[transparent, 0:3] = 0
    out = Path(path)
    tmp = out.with_suffix(".sanitized.png")
    Image.fromarray(arr, mode="RGBA").save(
        tmp,
        format="PNG",
        optimize=True,
        compress_level=6,
    )
    tmp.replace(out)


def main(argv: list[str]) -> int:
    path = Path(argv[0]) if len(argv) > 0 else DEFAULT
    if not path.is_file():
        print(f"missing {path}", file=sys.stderr)
        return 1
    sanitize(path)
    print(f"SANITIZE_OK {path} ({path.stat().st_size} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
