"""Yellow mask regression tests (synthetic frames, no Windows needed).
Run: python tests/test_color_mask.py"""
import os, sys
import numpy as np, cv2
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import types
# aimbot imports Windows-only modules; stub them for headless runs
for n in ("win32api", "win32con"):
    sys.modules.setdefault(n, types.SimpleNamespace(GetAsyncKeyState=lambda k: 0))
from aimbot import _color_mask

def rgb(frame, x0, y0, x1, y1, c):
    frame[y0:y1, x0:x1] = (c[2], c[1], c[0])          # frame is BGR

def mask_of(frame):
    return _color_mask(cv2.cvtColor(frame, cv2.COLOR_BGR2HSV), "yellow") > 0

def wall():
    f = np.zeros((120, 120, 3), np.uint8); f[:] = (110, 170, 210)      # tan, H~15
    rng = np.random.default_rng(1)
    # speckled highlights that reach H~22-26, V~190-200 (what leaked through before)
    for _ in range(300):
        x, y = rng.integers(0, 120, 2)
        f[y, x] = (95, 175 + rng.integers(0, 20), 200)
    return f

def test_wall_alone_is_empty():
    assert mask_of(wall()).sum() == 0, "tan wall must not produce target pixels"

def test_outline_found_on_wall():
    f = wall()
    for (x0, y0, x1, y1) in [(50, 40, 52, 80), (70, 40, 72, 80), (50, 40, 72, 42), (50, 78, 72, 80)]:
        rgb(f, x0, y0, x1, y1, (254, 254, 55))
    m = mask_of(f)
    ys, xs = np.nonzero(m)
    assert len(xs) >= 150
    assert 46 <= xs.min() and xs.max() <= 76 and 36 <= ys.min() and ys.max() <= 84, "mask leaked outside the bot"

def test_blurred_edge_kept_next_to_core():
    f = wall()
    rgb(f, 60, 30, 62, 90, (254, 254, 50))             # core
    rgb(f, 62, 30, 64, 90, (205, 205, 70))             # softened edge, V=205 (< core threshold)
    m = mask_of(f)
    assert m[60, 62] and m[60, 63], "edge pixels adjacent to the core must survive"

def test_loose_pixels_far_from_core_rejected():
    f = wall()
    rgb(f, 60, 30, 62, 90, (254, 254, 50))
    rgb(f, 10, 10, 14, 14, (205, 205, 70))             # same loose color, far from any core
    m = mask_of(f)
    assert not m[10:14, 10:14].any()

def test_other_colors_untouched():
    f = np.zeros((40, 40, 3), np.uint8); rgb(f, 5, 5, 15, 15, (250, 100, 250))
    assert (_color_mask(cv2.cvtColor(f, cv2.COLOR_BGR2HSV), "purple") > 0)[5:15, 5:15].any()

if __name__ == "__main__":
    fails = 0
    for k, v in sorted(globals().items()):
        if k.startswith("test_"):
            try: v(); print("PASS", k)
            except AssertionError as e: fails += 1; print("FAIL", k, e)
    sys.exit(1 if fails else 0)
