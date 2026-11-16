"""C++ aim engine port: mask, head pick, closed-loop convergence. Run: python tests/test_cbcore.py"""
import os, sys, random
import numpy as np, cv2
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import cbcore

def bgr(frame, x0, y0, x1, y1, rgb):
    frame[y0:y1, x0:x1] = (rgb[2], rgb[1], rgb[0])

def hsv_mask(frame, color="yellow"):
    return cbcore.build_mask(cv2.cvtColor(frame, cv2.COLOR_BGR2HSV), color)

def ring(f, cx, top, w, h, rgb=(254, 254, 55)):
    """1-2px outline rectangle = head, plus a wider body below."""
    x0, x1 = cx - w // 2, cx + w // 2
    bgr(f, x0, top, x1, top + 2, rgb); bgr(f, x0, top + h - 2, x1, top + h, rgb)
    bgr(f, x0, top, x0 + 2, top + h, rgb); bgr(f, x1 - 2, top, x1, top + h, rgb)
    bx0, bx1 = cx - w, cx + w                      # shoulders / body
    bgr(f, bx0, top + h, bx1, top + h + 2, rgb); bgr(f, bx0, top + h, bx0 + 2, top + h + 40, rgb)
    bgr(f, bx1 - 2, top + h, bx1, top + h + 40, rgb)

def test_brick_wall_not_masked_outline_is_():
    f = np.zeros((100, 100, 3), np.uint8)
    bgr(f, 0, 0, 50, 100, (200, 140, 60))          # orange brick  H~24
    bgr(f, 50, 0, 100, 100, (110, 170, 210)[::-1]) # tan
    assert hsv_mask(f)[:, :50].sum() == 0 and hsv_mask(f)[:, 50:].sum() == 0
    bgr(f, 20, 20, 30, 30, (254, 254, 55))
    assert hsv_mask(f)[20:30, 20:30].all()

def test_head_is_top_blob_part_not_body_centroid():
    f = np.zeros((200, 200, 3), np.uint8); ring(f, 100, 60, 14, 16)
    t = cbcore.find_target(hsv_mask(f), (100, 100))
    assert t is not None
    hx, top, hh = t[0], t[1], t[2]
    assert abs(hx - 100) <= 1.5 and abs(top - 60) <= 1, (hx, top)
    assert 8 <= hh <= 20, hh

def test_nearest_head_wins_and_noise_ignored():
    f = np.zeros((200, 300, 3), np.uint8); ring(f, 60, 50, 14, 16); ring(f, 220, 90, 14, 16)
    bgr(f, 150, 10, 152, 12, (254, 254, 55))       # 4 px speck < MIN_BLOB_AREA
    t = cbcore.find_target(hsv_mask(f), (150, 100))
    assert t[0] > 150, "head nearer the crosshair must win"

def test_no_pixels_no_move_and_idle_reset():
    a = cbcore.CppAim(random.Random(1)); m = np.zeros((100, 100), np.uint8)
    r = a.update(m, {}, 1.0); assert not r["found"] and r["mx"] == 0 and r["my"] == 0
    a.acquired = True; a.update(m, {}, 2.0); assert not a.acquired, "idle > 0.25s resets the lock"

def _sim(gain=1.0, lag_frames=2, start=(45, -35), seconds=2.0, seed=3):
    """Virtual crosshair vs a static head; mouse counts reach the screen after `lag_frames` frames
    (1 count = 1 px at gain 1). Returns (final_err, flips, moves)."""
    rng = random.Random(seed); a = cbcore.CppAim(rng)
    cfg = {"lock_gain": gain}
    off = list(map(float, start))                  # head minus crosshair, screen px
    pipe = [(0, 0)] * lag_frames; dt = 1 / 120.0; t = 10.0
    last_sign = (0, 0); flips = moves = 0
    while t < 10.0 + seconds:
        f = np.zeros((200, 200, 3), np.uint8)
        hx, hy = int(round(100 + off[0])), int(round(100 + off[1]))
        ring(f, hx, hy - 8, 14, 16)                # head centre = hy
        r = a.update(hsv_mask(f), cfg, t)
        mx, my = r["mx"], r["my"]
        if mx or my:
            moves += 1
            s = (np.sign(mx), np.sign(my))
            flips += int(last_sign != (0, 0) and ((s[0] and s[0] == -last_sign[0]) or (s[1] and s[1] == -last_sign[1])))
            last_sign = s
        pipe.append((mx, my)); px, py = pipe.pop(0)
        off[0] -= px / gain * gain; off[1] -= py / gain * gain   # counts -> px (1:1 here)
        t += dt
    return float(np.hypot(*off)), flips, moves

def test_converges_and_holds_with_latency():
    err, flips, moves = _sim()
    assert err < 6, f"final error {err:.1f}px"
    assert flips <= 3, f"{flips} direction flips = oscillation"

def test_converges_from_far_and_other_side():
    for st in [(-60, 40), (0, -70), (70, 0)]:
        err, flips, _ = _sim(start=st, seed=5)
        assert err < 6 and flips <= 3, (st, err, flips)

if __name__ == "__main__":
    fails = 0
    for k, v in sorted(globals().items()):
        if k.startswith("test_"):
            try: v(); print("PASS", k)
            except AssertionError as e: fails += 1; print("FAIL", k, e)
    sys.exit(1 if fails else 0)
