"""
aimbot.py — Improved aim logic
Same interface, same config, same color detection.
Changes: smooth movement, no jitter, human-like curves.
"""

import numpy as np
import cv2
import time
import ctypes
import json
import os
import random
import math

# ---------- config helpers ----------

_CFG_PATH = os.path.join(os.getenv("APPDATA", ""), "Config.Bura")


def _read_cfg_file() -> dict:
    try:
        with open(_CFG_PATH, "r") as f:
            return json.load(f)
    except Exception:
        return {}


def load_config() -> dict:
    return _read_cfg_file()


def save_key(key: str, value):
    cfg = _read_cfg_file()
    cfg[key] = value
    with open(_CFG_PATH, "w") as f:
        json.dump(cfg, f)


def load_key(key: str, default=None):
    cfg = _read_cfg_file()
    return cfg.get(key, default)


def apply_cfg(cfg: dict):
    with open(_CFG_PATH, "w") as f:
        json.dump(cfg, f)


# ---------- color ranges (HSV) ----------

COLOR_RANGES = {
    "yellow": {
        "lower": np.array([18, 120, 120], dtype=np.uint8),
        "upper": np.array([32, 255, 255], dtype=np.uint8),
    },
    "red": {
        "lower": np.array([0, 120, 120], dtype=np.uint8),
        "upper": np.array([10, 255, 255], dtype=np.uint8),
    },
    "purple": {
        "lower": np.array([125, 50, 50], dtype=np.uint8),
        "upper": np.array([155, 255, 255], dtype=np.uint8),
    },
}


# ---------- capture wrapper ----------

class _CaptureWrapper:
    def __init__(self, capture_obj):
        self._cap = capture_obj

    def read(self):
        return self._cap.capture_screen()

    def close(self):
        self._cap.close()


# ---------- smoothing engine ----------

class _SmoothState:
    """Tracks per-method smoothing state across frames."""
    __slots__ = (
        "vx", "vy",
        "prev_tx", "prev_ty",
        "last_time",
        "locked",
        "no_target_frames",
        "rx", "ry",  # sub-pixel residual
    )

    def __init__(self):
        self.vx = 0.0
        self.vy = 0.0
        self.prev_tx = 0.0
        self.prev_ty = 0.0
        self.last_time = time.perf_counter()
        self.locked = True
        self.no_target_frames = 0
        self.rx = 0.0
        self.ry = 0.0

    def decay_idle(self, factor=0.4):
        self.vx *= factor
        self.vy *= factor

    def reset(self):
        self.vx = 0.0
        self.vy = 0.0
        self.rx = 0.0
        self.ry = 0.0
        self.locked = True


def _smooth_move(st: _SmoothState, dx: float, dy: float,
                 spd_x: float, spd_y: float,
                 dz_x: int, dz_y: int,
                 smooth: float = 0.4,
                 humanize: bool = True) -> tuple:
    """
    Smoothing pipeline:
    1. Deadzone hysteresis — inner/outer threshold prevents boundary chatter
    2. Acceleration curve — slow start, fast middle, slow end (like a real hand)
    3. EMA velocity — blends across frames, kills zigzag
    4. Sub-pixel accumulator — fractional moves carry over
    5. Gaussian micro-noise — not uniform random, feels organic
    """
    now = time.perf_counter()
    dt = min(now - st.last_time, 0.05)
    st.last_time = now

    adx, ady = abs(dx), abs(dy)
    dist = math.hypot(dx, dy)

    # ---- deadzone hysteresis ----
    outer_x, outer_y = dz_x * 2.0, dz_y * 2.0
    if st.locked:
        if adx < outer_x and ady < outer_y:
            st.decay_idle(0.2)
            st.rx = st.ry = 0.0
            return (0, 0)
        st.locked = False
    else:
        if adx < dz_x and ady < dz_y:
            st.reset()
            return (0, 0)

    # ---- acceleration curve (ease-in-out) ----
    # humans: slow start → peak speed → decelerate near target
    # using smoothstep: 3t² - 2t³
    max_d = 100.0
    t = min(dist / max_d, 1.0)
    accel = t * t * (3.0 - 2.0 * t)  # smoothstep 0→1→0-ish
    # but we want it to be slower when very close, so blend:
    # close (t<0.15): very slow, mid: fast, far: moderate
    speed_mult = 0.15 + 0.85 * accel

    want_vx = (dx / spd_x) * speed_mult
    want_vy = (dy / spd_y) * speed_mult

    # ---- EMA blend (frame-rate independent) ----
    # alpha scales with dt so behavior is consistent across fps
    alpha = 1.0 - math.pow(1.0 - smooth, dt * 120.0)
    alpha = max(0.03, min(0.9, alpha))

    st.vx += (want_vx - st.vx) * alpha
    st.vy += (want_vy - st.vy) * alpha

    # ---- gaussian micro-noise (human hand tremor) ----
    # scaled to velocity magnitude — still when near target, slight when moving
    if humanize:
        vmag = math.hypot(st.vx, st.vy)
        if vmag > 0.5:
            noise_scale = min(vmag * 0.06, 0.8)
            st.vx += random.gauss(0, noise_scale)
            st.vy += random.gauss(0, noise_scale)

    # ---- sub-pixel accumulation ----
    st.rx += st.vx
    st.ry += st.vy

    ix = int(st.rx)
    iy = int(st.ry)
    st.rx -= ix
    st.ry -= iy

    return (ix, iy)


def _pick_target(xs, ys, cx, cy, prev_x, prev_y, has_prev: bool) -> tuple:
    """
    Target selection: head-cluster centroid with persistence.
    - Takes top 15% of pixels (head region)
    - Centroid of that cluster (stable, not noisy)
    - Blends 25% toward previous target to reduce frame-to-frame snap
    """
    if len(xs) == 0:
        return (cx, cy, False)

    # Head cluster: topmost 15%
    thresh = np.percentile(ys, 15)
    mask = ys <= thresh
    hx, hy = xs[mask], ys[mask]

    if len(hx) == 0:
        hx, hy = xs, ys

    tx = float(np.mean(hx))
    ty = float(np.mean(hy))

    # Persistence blend — only if target didn't teleport
    if has_prev:
        d = math.hypot(tx - prev_x, ty - prev_y)
        if d < 25:
            tx = tx * 0.75 + prev_x * 0.25
            ty = ty * 0.75 + prev_y * 0.25

    return (tx, ty, True)


# ---------- aimbot class ----------

class aimbot:

    def __init__(self, mouse, capture, cfg: dict):
        self.mouse = mouse
        self.capture = _CaptureWrapper(capture)
        self.cfg = cfg

        self.key = 0
        self.tol = cfg.get("tolerance", 0)
        self.state = False
        self.key_state = False

        self._st_trigger = _SmoothState()
        self._st_flick = _SmoothState()
        self._st_magnet = _SmoothState()

        self._prev_tx = 0.0
        self._prev_ty = 0.0
        self._has_prev = False

        self._recoil_start = 0.0
        self._firing = False

    # ---------- trigger (main aimbot) ----------
    def trigger(self, cfg: dict = None):
        if cfg:
            self.cfg = cfg

        frame = self.capture.read()
        if frame is None:
            return

        tag = self.cfg.get("color_to_track", "yellow")
        tol = int(self.cfg.get("tolerance", 0))
        cr = COLOR_RANGES.get(tag, COLOR_RANGES["yellow"])
        lo, hi = cr["lower"].copy(), cr["upper"].copy()

        if tol > 0:
            lo = np.clip(lo.astype(int) - tol, 0, 255).astype(np.uint8)
            hi = np.clip(hi.astype(int) + tol, 0, 255).astype(np.uint8)

        fh, fw = frame.shape[0], frame.shape[1]
        cy, cx = fh // 2, fw // 2
        hx = int(self.cfg.get("fovx", 50))
        hy = int(self.cfg.get("fovy", 50))

        roi = frame[max(0, cy-hy):min(fh, cy+hy), max(0, cx-hx):min(fw, cx+hx)]
        if roi.size == 0:
            return

        hsv = cv2.cvtColor(roi, cv2.COLOR_BGR2HSV)
        mask = cv2.inRange(hsv, lo, hi)
        ys, xs = np.nonzero(mask)

        rcx, rcy = roi.shape[1] // 2, roi.shape[0] // 2

        if len(xs) == 0:
            self._st_trigger.no_target_frames += 1
            if self._st_trigger.no_target_frames > 5:
                self._has_prev = False
            self._st_trigger.decay_idle(0.4)
            return

        self._st_trigger.no_target_frames = 0

        tx, ty, found = _pick_target(xs, ys, rcx, rcy, self._prev_tx, self._prev_ty, self._has_prev)
        self._prev_tx, self._prev_ty = tx, ty
        self._has_prev = found

        offset = int(self.cfg.get("offset", 0))
        ty += offset

        raw_dx = tx - rcx
        raw_dy = ty - rcy

        sx = max(1.0, float(self.cfg.get("speedX", 3)))
        sy = max(1.0, float(self.cfg.get("speedY", 3)))
        dzx = int(self.cfg.get("deadzone_x", 3))
        dzy = int(self.cfg.get("deadzone_y", 3))

        sm = 0.4
        if self.cfg.get("legitmode", False):
            sm = 0.22

        hum = bool(self.cfg.get("humanize", True))
        mx, my = _smooth_move(self._st_trigger, raw_dx, raw_dy, sx, sy, dzx, dzy, smooth=sm, humanize=hum)

        if self.cfg.get("anti_strafe", False):
            mx = int(mx * 0.65)

        if self.cfg.get("legitmode", False) and (abs(mx) > 5 or abs(my) > 5):
            mx = int(math.copysign(5 + math.log1p(abs(mx) - 5), mx)) if abs(mx) > 5 else mx
            my = int(math.copysign(5 + math.log1p(abs(my) - 5), my)) if abs(my) > 5 else my

        if mx != 0 or my != 0:
            self.mouse.move(mx, my)

        # Recoil
        if self.cfg.get("recoil_auto", False):
            rx = float(self.cfg.get("recoil_x", 0))
            ry = float(self.cfg.get("recoil_y", 0))
            dur = float(self.cfg.get("recoil_duration", 0))
            if rx != 0 or ry != 0:
                now = time.perf_counter()
                if not self._firing:
                    self._recoil_start = now
                    self._firing = True
                if dur <= 0 or (now - self._recoil_start) < dur:
                    self.mouse.move(int(rx), int(ry))
                else:
                    self._firing = False

        # Triggerbot
        if self.cfg.get("magnet_triggerbot", False):
            pd = int(self.cfg.get("pixel_distance", 5))
            if abs(raw_dx) < pd and abs(raw_dy) < pd:
                d = float(self.cfg.get("base_delay", 0.05))
                d += random.gauss(0, d * 0.1)
                time.sleep(max(0.01, d))
                self.mouse.click()

        # Config is pushed from Flask route, no per-frame disk read

    # ---------- spinbot ----------
    def spinbot(self, cfg: dict = None):
        if cfg:
            self.cfg = cfg

        frame = self.capture.read()
        if frame is None:
            return

        cx, cy = frame.shape[1] // 2, frame.shape[0] // 2
        tag = self.cfg.get("color_to_track", "yellow")
        cr = COLOR_RANGES.get(tag, COLOR_RANGES["yellow"])

        hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
        mask = cv2.inRange(hsv, cr["lower"], cr["upper"])
        ys, xs = np.nonzero(mask)
        if len(xs) > 0:
            dx = int(np.mean(xs)) - cx
            self.mouse.move(int(dx * float(self.cfg.get("game_sens", 1.0))), 0)

    # ---------- magnet ----------
    def magnet(self, cfg: dict = None):
        if cfg:
            self.cfg = cfg

        # Key check already done by _magnet_should_fire() in Main.py

        frame = self.capture.read()
        if frame is None:
            return

        H, W = frame.shape[0], frame.shape[1]
        bh = int(self.cfg.get("magnet_fov", 50))
        cx, cy = W // 2, H // 2

        crop = frame[max(0, cy-bh):min(H, cy+bh), max(0, cx-bh):min(W, cx+bh)]
        if crop.size == 0:
            return

        tag = self.cfg.get("color_to_track", "yellow")
        cr = COLOR_RANGES.get(tag, COLOR_RANGES["yellow"])

        hsv = cv2.cvtColor(crop, cv2.COLOR_BGR2HSV)
        mask = cv2.inRange(hsv, cr["lower"], cr["upper"])
        ys, xs = np.nonzero(mask)

        if len(xs) == 0:
            self._st_magnet.no_target_frames += 1
            self._st_magnet.decay_idle(0.4)
            return

        self._st_magnet.no_target_frames = 0
        ccx, ccy = crop.shape[1] // 2, crop.shape[0] // 2

        dists = np.sqrt((xs - ccx) ** 2 + (ys - ccy) ** 2)
        ci = np.argmin(dists)

        rdx = float(xs[ci] - ccx)
        rdy = float(ys[ci] - ccy)

        strength = float(self.cfg.get("magnet_strength", 0.5))
        sm = max(1.0, float(self.cfg.get("magnet_smooth", 3.0)))

        mx, my = _smooth_move(
            self._st_magnet,
            rdx * strength, rdy * strength,
            sm, sm, 2, 2,
            smooth=0.3
        )

        if mx != 0 or my != 0:
            self.mouse.move(mx, my)
