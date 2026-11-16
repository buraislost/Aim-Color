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
import threading
import collections

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
        "lower": np.array([18, 100, 120], dtype=np.uint8),
        "upper": np.array([34, 255, 255], dtype=np.uint8),
    },
    "red": {
        "lower": np.array([0, 120, 120], dtype=np.uint8),
        "upper": np.array([10, 255, 255], dtype=np.uint8),
        # red wraps around the hue circle — second band at the top end
        "lower2": np.array([170, 120, 120], dtype=np.uint8),
        "upper2": np.array([179, 255, 255], dtype=np.uint8),
    },
    "purple": {
        "lower": np.array([125, 50, 50], dtype=np.uint8),
        "upper": np.array([155, 255, 255], dtype=np.uint8),
    },
}


# ---------- mask helpers ----------

def _color_mask(hsv, tag: str, tol: int = 0):
    """inRange over every band of the color (red has two), tolerance widens each."""
    cr = COLOR_RANGES.get(tag, COLOR_RANGES["yellow"])
    bands = [(cr["lower"], cr["upper"])]
    if "lower2" in cr:
        bands.append((cr["lower2"], cr["upper2"]))
    mask = None
    for lo, hi in bands:
        lo = lo.astype(int)
        hi = hi.astype(int)
        if tol > 0:
            # Tolerance loosens saturation/value freely, but hue only by a hair:
            # widening hue by `tol` drags orange doors / brick (H~11) into yellow.
            ht = min(tol, 2)
            lo = lo - np.array([ht, tol, tol])
            hi = hi + np.array([ht, tol, tol])
        lo = np.clip(lo, 0, 255).astype(np.uint8)
        hi = np.clip(hi, 0, 255).astype(np.uint8)
        m = cv2.inRange(hsv, lo, hi)
        mask = m if mask is None else cv2.bitwise_or(mask, m)
    return mask


_KERNEL = np.ones((5, 5), np.uint8)


def _select_blob(mask, min_area: int = 8):
    """
    Keep only the color blob closest to the ROI center. Outline fragments are
    glued with a dilate first; specks and far-away background noise are dropped
    so they can't drag the head-cluster centroid around.
    Returns (ys, xs) of the chosen blob's original mask pixels.
    """
    ys, xs = np.nonzero(mask)
    if len(xs) == 0:
        return ys, xs
    n, labels, _, cents = cv2.connectedComponentsWithStats(
        cv2.dilate(mask, _KERNEL), connectivity=8)
    cnt = np.bincount(labels[ys, xs], minlength=n)      # real mask pixels per blob
    cnt[0] = 0
    # drop specks, and anything much smaller than the biggest blob
    floor = max(min_area, int(cnt.max() * 0.25))
    rcx, rcy = mask.shape[1] / 2.0, mask.shape[0] / 2.0
    best, best_d = -1, 1e18
    for k in range(1, n):
        if cnt[k] < floor:
            continue
        d = (cents[k][0] - rcx) ** 2 + (cents[k][1] - rcy) ** 2
        if d < best_d:
            best, best_d = k, d
    if best < 0:
        return ys[:0], xs[:0]
    sel = labels[ys, xs] == best
    return ys[sel], xs[sel]


def _render_png(roi, mask, target=None, ext=".png") -> bytes:
    """ROI | mask side by side, crosshair + target marker, as PNG bytes."""
    out = np.hstack([np.ascontiguousarray(roi), cv2.cvtColor(mask, cv2.COLOR_GRAY2BGR)])
    h = out.shape[0]
    s = max(1, 240 // h) if h <= 240 else 240.0 / h
    out = cv2.resize(out, None, fx=s, fy=s,
                     interpolation=cv2.INTER_NEAREST if s >= 1 else cv2.INTER_AREA)
    w2 = int(roi.shape[1] * s)
    cx, cy = int(roi.shape[1] // 2 * s), int(roi.shape[0] // 2 * s)
    for ox in (0, w2):
        cv2.drawMarker(out, (ox + cx, cy), (255, 255, 255), cv2.MARKER_CROSS, 12, 1)
        if target:
            cv2.circle(out, (ox + int(target[0] * s), int(target[1] * s)), 6, (0, 255, 0), 1)
    ok, buf = (cv2.imencode(ext, out, [cv2.IMWRITE_JPEG_QUALITY, 80]) if ext == ".jpg"
               else cv2.imencode(ext, out))
    return buf.tobytes() if ok else b""


def _roi_mask(frame, cfg: dict):
    """Crop the FOV box around the frame center and build the color mask."""
    fh, fw = frame.shape[0], frame.shape[1]
    cy, cx = fh // 2, fw // 2
    hx = int(cfg.get("fovx", 50))
    hy = int(cfg.get("fovy", 50))
    roi = frame[max(0, cy-hy):min(fh, cy+hy), max(0, cx-hx):min(fw, cx+hx)]
    if roi.size == 0:
        return None, None
    hsv = cv2.cvtColor(roi, cv2.COLOR_BGR2HSV)
    mask = _color_mask(hsv, cfg.get("color_to_track", "yellow"),
                       int(cfg.get("tolerance", 0)))
    return roi, mask


def wide_scan(img, px: int, py: int, hx: int, hy: int, cfg: dict) -> dict:
    """Color pixels over the whole wide view, split into inside / outside the FOV box.
    Tells the user *why* nothing is tracked: no color at all, or color outside the FOV."""
    hsv = cv2.cvtColor(img, cv2.COLOR_BGR2HSV)
    mask = _color_mask(hsv, cfg.get("color_to_track", "yellow"), int(cfg.get("tolerance", 0)))
    h, w = mask.shape
    box = np.zeros(mask.shape, dtype=bool)
    box[max(0, py - hy):min(h, py + hy), max(0, px - hx):min(w, px + hx)] = True
    res = {"in": int(np.count_nonzero(mask[box])), "out": 0,
           "dx": None, "dy": None, "need": None, "t": time.time()}
    out = mask.copy()
    out[box] = 0
    ys, xs = np.nonzero(out)
    res["out"] = int(len(xs))
    if len(xs) == 0:
        return res
    n, labels, _, cents = cv2.connectedComponentsWithStats(cv2.dilate(out, _KERNEL), connectivity=8)
    cnt = np.bincount(labels[ys, xs], minlength=n)
    cnt[0] = 0
    floor = max(int(cfg.get("min_blob", 8)), int(cnt.max() * 0.25))
    best, best_d = -1, 1e18
    for k in range(1, n):
        if cnt[k] < floor:
            continue
        d = (cents[k][0] - px) ** 2 + (cents[k][1] - py) ** 2
        if d < best_d:
            best, best_d = k, d
    if best >= 0:
        sel = labels[ys, xs] == best
        bx, by = xs[sel], ys[sel]
        res["dx"] = int(round(bx.mean() - px))
        res["dy"] = int(round(by.mean() - py))
        res["need"] = int(max(np.abs(bx - px).max(), np.abs(by - py).max()) + 6)
    return res


# ---------- capture wrapper ----------

class _CaptureWrapper:
    def __init__(self, capture_obj):
        self._cap = capture_obj
        self._lock = threading.Lock()  # aim loop + debug endpoint share the grabber

    def read(self):
        with self._lock:
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
        "why",       # last decision, for the debug trace
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
        self.why = ""

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
                 smooth: float = 0.4) -> tuple:
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
            st.why = "deadzone(locked)"
            return (0, 0)
        st.locked = False
    else:
        if adx < dz_x and ady < dz_y:
            st.reset()
            st.why = "deadzone"
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

    # Speeds were tuned for a ~1 kHz loop. The DXGI grab paces the loop to the
    # capture fps (~60 Hz), which made the pull ~16x slower. Scale by the real
    # frame time so px/s stays constant, and never step past a quarter of the error.
    k = max(1.0, min(dt * 1000.0, 16.0))
    want_vx = (dx / spd_x) * speed_mult * k
    want_vy = (dy / spd_y) * speed_mult * k
    cap_x = max(1.0, 0.25 * adx)
    cap_y = max(1.0, 0.25 * ady)
    want_vx = max(-cap_x, min(cap_x, want_vx))
    want_vy = max(-cap_y, min(cap_y, want_vy))

    # ---- EMA blend (frame-rate independent) ----
    # alpha scales with dt so behavior is consistent across fps
    alpha = 1.0 - math.pow(1.0 - smooth, dt * 120.0)
    alpha = max(0.03, min(0.9, alpha))

    st.vx += (want_vx - st.vx) * alpha
    st.vy += (want_vy - st.vy) * alpha

    # ---- gaussian micro-noise (human hand tremor) ----
    # scaled to velocity magnitude — still when near target, slight when moving
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

    st.why = "move" if (ix or iy) else "subpixel"
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

        self.n_moves = 0
        self.dbg = {}
        self.log = collections.deque(maxlen=800)   # (t, why, px, dx, dy, mx, my)
        self.hit = None                            # last frame that had the color
        self.wide_info = None                      # color scan of the wide debug view
        self._hit_t = 0.0

    # ---------- debug (never moves the mouse) ----------
    def peek(self, cfg: dict = None):
        """Detection snapshot for the WebGUI debug tab. Returns (info, roi, mask)."""
        cfg = cfg or self.cfg
        frame = self.capture.read()
        if frame is None:
            return {"err": "capture returned None"}, None, None
        fh, fw = frame.shape[0], frame.shape[1]
        info = {
            "frame": [int(fw), int(fh)],
            "mean": round(float(frame[::8, ::8].mean()), 2),
            "px": 0, "target": None, "dx": None, "dy": None,
            "moves": self.n_moves,
            "last_move": [self.dbg.get("mx"), self.dbg.get("my")],
        }
        roi, mask = _roi_mask(frame, cfg)
        if roi is None:
            info["err"] = "FOV box is empty"
            return info, None, None
        info["mask_px"] = int(np.count_nonzero(mask))
        ys, xs = _select_blob(mask, int(cfg.get("min_blob", 8)))
        rcx, rcy = roi.shape[1] // 2, roi.shape[0] // 2
        info["roi"] = [int(roi.shape[1]), int(roi.shape[0])]
        info["px"] = int(len(xs))
        if len(xs):
            tx, ty, _ = _pick_target(xs, ys, rcx, rcy, 0.0, 0.0, False)
            ty += int(cfg.get("offset", 0))
            info["target"] = [round(tx, 1), round(ty, 1)]
            info["dx"] = round(tx - rcx, 1)
            info["dy"] = round(ty - rcy, 1)
        return info, roi, mask

    def debug_png(self, cfg: dict = None):
        info, roi, mask = self.peek(cfg)
        if roi is None:
            return None
        return _render_png(roi, mask, info.get("target"))

    def debug_jpg(self, cfg: dict = None):
        info, roi, mask = self.peek(cfg)
        if roi is None:
            return None
        return _render_png(roi, mask, info.get("target"), ".jpg")

    def debug_wide_jpg(self, cfg: dict = None, center=None):
        """Wide look around the crosshair: FOV box, detected color pixels and the picked target."""
        from Capture import grab_region, screen_size
        cfg = cfg or self.cfg
        sw, sh = screen_size()
        vw = max(64, min(int(cfg.get("view_w", 720)), sw))
        vh = max(64, min(int(cfg.get("view_h", 320)), sh))
        cx, cy = center if center else (sw // 2, sh // 2)
        x0 = max(0, min(sw - vw, cx - vw // 2))
        y0 = max(0, min(sh - vh, cy - vh // 2))
        img = grab_region(x0, y0, vw, vh)
        px, py = cx - x0, cy - y0
        hx, hy = int(cfg.get("fovx", 50)), int(cfg.get("fovy", 50))
        rx0, rx1 = max(0, px - hx), min(vw, px + hx)
        ry0, ry1 = max(0, py - hy), min(vh, py + hy)
        try:
            self.wide_info = wide_scan(img, px, py, hx, hy, cfg)
        except Exception as e:
            self.wide_info = {"err": f"{type(e).__name__}: {e}", "t": time.time()}
        roi = img[ry0:ry1, rx0:rx1]
        if roi.size:
            hsv = cv2.cvtColor(roi, cv2.COLOR_BGR2HSV)
            mask = _color_mask(hsv, cfg.get("color_to_track", "yellow"), int(cfg.get("tolerance", 0)))
            roi[mask > 0] = (255, 0, 255)                      # detected pixels (in place on img)
            ys, xs = _select_blob(mask, int(cfg.get("min_blob", 8)))
            if len(xs):
                tx, ty, _ = _pick_target(xs, ys, roi.shape[1] // 2, roi.shape[0] // 2, 0.0, 0.0, False)
                ty += int(cfg.get("offset", 0))
                cv2.circle(img, (rx0 + int(tx), ry0 + int(ty)), 7, (0, 255, 0), 1)
        cv2.rectangle(img, (px - hx, py - hy), (px + hx, py + hy), (60, 255, 60), 1)
        cv2.line(img, (px - 8, py), (px + 8, py), (255, 255, 255), 1)
        cv2.line(img, (px, py - 8), (px, py + 8), (255, 255, 255), 1)
        ok, buf = cv2.imencode(".jpg", img, [cv2.IMWRITE_JPEG_QUALITY, 80])
        return buf.tobytes() if ok else None

    def summary(self, window: float = 5.0) -> dict:
        """What the aim loop actually decided in the last `window` seconds."""
        now = time.perf_counter()
        rows = [r for r in list(self.log) if now - r[0] <= window]
        mix = {}
        for r in rows:
            mix[r[1]] = mix.get(r[1], 0) + 1
        last = rows[-1] if rows else None
        return {
            "calls": len(rows),
            "rate": round(len(rows) / window, 1),
            "reasons": mix,
            "last": ({"why": last[1], "px": last[2], "dx": last[3], "dy": last[4],
                      "mx": last[5], "my": last[6], "age": round(now - last[0], 2)}
                     if last else None),
            "sent": [sum(abs(r[5]) for r in rows), sum(abs(r[6]) for r in rows)],
            "hit_t": self.hit["t"] if self.hit else None,
            "hit_age": round(time.time() - self.hit["t"], 1) if self.hit else None,
        }

    # ---------- trigger (main aimbot) ----------
    def trigger(self, cfg: dict = None):
        if cfg:
            self.cfg = cfg

        frame = self.capture.read()
        if frame is None:
            return

        ts = time.perf_counter()
        roi, mask = _roi_mask(frame, self.cfg)
        if roi is None:
            return
        ys, xs = _select_blob(mask, int(self.cfg.get("min_blob", 8)))

        rcx, rcy = roi.shape[1] // 2, roi.shape[0] // 2

        if len(xs) == 0:
            self.log.append((ts, "no_pixels", 0, 0.0, 0.0, 0, 0))
            self._st_trigger.no_target_frames += 1
            if self._st_trigger.no_target_frames > 5:
                self._has_prev = False
            self._st_trigger.decay_idle(0.4)
            return

        self._st_trigger.no_target_frames = 0

        tx, ty, found = _pick_target(xs, ys, rcx, rcy, self._prev_tx, self._prev_ty, self._has_prev)
        self._prev_tx, self._prev_ty = tx, ty
        self._has_prev = found

        if ts - self._hit_t > 0.4:
            self._hit_t = ts
            self.hit = {"t": time.time(), "png": _render_png(roi, mask, (tx, ty))}

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

        mx, my = _smooth_move(self._st_trigger, raw_dx, raw_dy, sx, sy, dzx, dzy, smooth=sm)

        if self.cfg.get("anti_strafe", False):
            # ceil so a 1px step isn't truncated to 0 (x would never move at low speed)
            mx = int(math.copysign(math.ceil(abs(mx) * 0.65), mx))

        if self.cfg.get("legitmode", False) and (abs(mx) > 5 or abs(my) > 5):
            mx = int(math.copysign(5 + math.log1p(abs(mx) - 5), mx)) if abs(mx) > 5 else mx
            my = int(math.copysign(5 + math.log1p(abs(my) - 5), my)) if abs(my) > 5 else my

        self.dbg = {"mx": int(mx), "my": int(my)}
        self.log.append((ts, self._st_trigger.why, int(len(xs)), round(raw_dx, 1),
                         round(raw_dy, 1), int(mx), int(my)))
        if mx != 0 or my != 0:
            self.mouse.move(mx, my)
            self.n_moves += 1

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
        mask = _color_mask(hsv, tag)
        ys, xs = np.nonzero(mask)
        if len(xs) > 0:
            dx = int(np.mean(xs)) - cx
            self.mouse.move(int(dx * float(self.cfg.get("game_sens", 1.0))), 0)

    # ---------- magnet ----------
    def magnet(self, cfg: dict = None):
        if cfg:
            self.cfg = cfg

        key = int(self.cfg.get("magnet_key", 0x02))
        rs = ctypes.windll.user32.GetAsyncKeyState(key)
        if not (rs & 0x8000):
            self._st_magnet.decay_idle(0.3)
            return

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
        mask = _color_mask(hsv, tag)
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
