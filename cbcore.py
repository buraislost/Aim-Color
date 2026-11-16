"""Aim engine ported from the C++ ColorBot (Aim-Color main, val/src/Core/Features/ColorBot.cpp).
Pure numpy/cv2, no I/O: feed it the colour mask of the FOV box, it returns the mouse step.
Pipeline: mask (+close) -> head per blob -> low-passed aim point -> hold box -> reaction delay
-> error minus corrections still in flight -> ease -> +-10% -> carry fractions."""
import math
import random
from collections import deque

import cv2
import numpy as np

DEADZONE_PX = 0.8
MIN_BLOB_AREA = 12
HEAD_H_RATIO = 1.15
HEAD_MAX_FRAC = 0.45
HOLD_RELEASE_MUL = 1.6
ANCHOR_RADIUS = 60.0
AIM_FILTER_MS = 18.0
JUMP_PX = 40.0
LOST_MS = 120.0
PENDING_MS = 24.0
EASE_MIN = 0.35
IDLE_RESET_S = 0.25            # aim key released for this long: next press starts fresh

_CLOSE = np.ones((3, 3), np.uint8)

# OpenCV hue 0-179. Yellow outline measured H 26-32 (peak 30); H 24-25 is the Range's orange
# brick wall, which merged into the head blob and pulled the aim point up ~22 px.
RANGES = {
    "yellow": [((26, 90, 90), (34, 255, 255))],
    "purple": [((140, 80, 100), (170, 255, 255))],
    "red": [((0, 130, 110), (8, 255, 255)), ((172, 130, 110), (179, 255, 255))],
}


def build_mask(hsv, color: str):
    mask = None
    for lo, hi in RANGES.get(color, RANGES["yellow"]):
        m = cv2.inRange(hsv, np.array(lo, np.uint8), np.array(hi, np.uint8))
        mask = m if mask is None else cv2.bitwise_or(mask, m)
    # bridge 1-2 px gaps in the outline so one enemy stays one blob
    return cv2.morphologyEx(mask, cv2.MORPH_CLOSE, _CLOSE, iterations=2)


def find_target(mask, center, anchor=None):
    """Head nearest the crosshair (or nearest `anchor` while holding).
    Per blob: top = first row, head width = widest row among rows 3-8 below the top,
    head height = width * 1.15 (<= 45% of the blob), head x = centre of that row.
    Returns (head_x, top_y, head_h, n_blobs, n_big) or None."""
    n, labels, stats, _ = cv2.connectedComponentsWithStats(mask, connectivity=8)
    best, best_d, big = None, float("inf"), 0
    for i in range(1, n):
        if stats[i, cv2.CC_STAT_AREA] < MIN_BLOB_AREA:
            continue
        big += 1
        left, top = int(stats[i, cv2.CC_STAT_LEFT]), int(stats[i, cv2.CC_STAT_TOP])
        wd, ht = int(stats[i, cv2.CC_STAT_WIDTH]), int(stats[i, cv2.CC_STAT_HEIGHT])
        r0, r1 = top + min(3, ht - 1), top + min(8, ht - 1)
        bw, bcx = 0, 0.0
        for y in range(r0, r1 + 1):
            xs = np.flatnonzero(labels[y, left:left + wd] == i)
            if xs.size == 0:
                continue
            w = int(xs[-1] - xs[0]) + 1
            if w > bw:
                bw, bcx = w, left + (xs[0] + xs[-1]) * 0.5
        if bw == 0:
            continue
        head_h = min(max(min(bw * HEAD_H_RATIO, ht * HEAD_MAX_FRAC), 3.0), 120.0)
        if anchor is not None:
            d = math.hypot(bcx - anchor[0], top - anchor[1])
            if d > ANCHOR_RADIUS:
                continue
        else:
            d = math.hypot(bcx - center[0], top + head_h * 0.5 - center[1])
        if d < best_d:
            best_d, best = d, (bcx, float(top), head_h)
    if best is None:
        return None
    return best + (n - 1, big)


class CppAim:
    def __init__(self, rng=None):
        self.rng = rng or random.Random()
        self.reset()

    def reset(self):
        self.holding = False
        self.acquired = False
        self.filter_valid = False
        self.rem_x = self.rem_y = 0.0
        self.sent = deque()
        self.hold_head = (0.0, 0.0)
        self.aim_f = (0.0, 0.0)
        self.head_h_f = 0.0
        self.filter_t = 0.0
        self.last_seen = -1e9
        self.last_move = -1e9
        self.last_call = -1e9
        self.acquired_at = 0.0
        self.reaction = 0.0

    def update(self, mask, cfg, now, px_scale=1.0):
        """-> dict(found, why, mx, my, dx, dy, head, head_h, inside). `now` in seconds."""
        if now - self.last_call > IDLE_RESET_S:
            self.reset()
        self.last_call = now
        h, w = mask.shape[:2]
        center = (w // 2, h // 2)
        ps = max(0.5, float(px_scale))

        tgt = find_target(mask, center, self.hold_head if self.holding else None)
        out = {"found": tgt is not None, "why": "no_pixels", "mx": 0, "my": 0, "dx": 0.0, "dy": 0.0,
               "head": None, "head_h": 0.0, "inside": False}
        if tgt is None:
            if (now - self.last_seen) * 1000.0 > LOST_MS:
                self.holding = self.acquired = self.filter_valid = False
                self.rem_x = self.rem_y = 0.0
                self.sent.clear()
            return out

        hx, top, head_h = tgt[0], tgt[1], tgt[2]
        self.last_seen = now
        raw = (hx, top + head_h * 0.5)
        if not self.filter_valid or math.hypot(raw[0] - self.aim_f[0], raw[1] - self.aim_f[1]) > JUMP_PX * ps:
            self.aim_f, self.head_h_f, self.filter_valid = raw, head_h, True
        else:
            a = 1.0 - math.exp(-(now - self.filter_t) * 1000.0 / AIM_FILTER_MS)
            self.aim_f = (self.aim_f[0] + (raw[0] - self.aim_f[0]) * a, self.aim_f[1] + (raw[1] - self.aim_f[1]) * a)
            self.head_h_f += (head_h - self.head_h_f) * a
        self.filter_t = now

        ox = float(cfg.get("offset_x", 0)) * ps
        oy = float(cfg.get("offset", cfg.get("offset_y", 0))) * ps
        dx = self.aim_f[0] + ox - center[0]
        dy = self.aim_f[1] + oy - center[1]
        hh = self.head_h_f
        hold_pct = max(0, min(150, int(cfg.get("hold_pct", 60))))
        hold_half = max(2.0 * ps, hold_pct / 100.0 * 0.5 * hh)
        # hold / release are judged on where the crosshair WILL be (corrections still in flight
        # applied), not where it is drawn: otherwise the hold starts early and the in-flight counts
        # carry the crosshair past the box edge.
        gain = max(0.1, min(5.0, float(cfg.get("lock_gain", 1.0))))
        self._prune(now)
        pdx = dx - sum(q[1] for q in self.sent) / gain
        pdy = dy - sum(q[2] for q in self.sent) / gain
        inside = abs(pdx) <= hold_half and abs(pdy) <= hold_half
        out.update(dx=dx, dy=dy, head=(hx, top), head_h=hh, inside=inside)

        if self.holding:
            self.hold_head = (hx, top)
            if abs(pdx) > hold_half * HOLD_RELEASE_MUL or abs(pdy) > hold_half * HOLD_RELEASE_MUL:
                self.holding = False
        elif hold_pct > 0 and inside:
            self.holding, self.hold_head = True, (hx, top)

        if not self.acquired:
            self.acquired, self.acquired_at = True, now
            self.reaction = self.rng.uniform(70.0, 130.0) / 1000.0

        if self.holding:
            out["why"] = "hold"
            return out
        mv = self._aim(dx, dy, hh, cfg, now, ps)
        out["why"] = mv[2]
        out["mx"], out["my"] = mv[0], mv[1]
        return out

    def _prune(self, now):
        while self.sent and (now - self.sent[0][0]) * 1000.0 > PENDING_MS:
            self.sent.popleft()

    def _aim(self, dx, dy, head_h, cfg, now, ps):
        if now - self.acquired_at < self.reaction:
            return 0, 0, "reaction"
        settle = max(1, min(30, int(cfg.get("lock_settle_ms", 10)))) / 1000.0
        if now - self.last_move < settle:
            return 0, 0, "settle"
        self.last_move = now
        gain = max(0.1, min(5.0, float(cfg.get("lock_gain", 1.0))))
        smooth = max(0.05, min(1.0, float(cfg.get("cpp_smooth", 0.5))))
        self._prune(now)
        px = sum(s[1] for s in self.sent)
        py = sum(s[2] for s in self.sent)
        ex, ey = dx - px / gain, dy - py / gain      # error vs where the crosshair WILL be
        dist = math.hypot(ex, ey)
        if dist < DEADZONE_PX * ps:
            self.rem_x = self.rem_y = 0.0
            return 0, 0, "deadzone"
        ref = max(8.0 * ps, 1.5 * head_h)
        ease = max(EASE_MIN, min(1.0, dist / ref))
        k = gain * smooth * ease * self.rng.uniform(0.9, 1.1)
        fx, fy = ex * k + self.rem_x, ey * k + self.rem_y
        ix, iy = int(math.floor(fx + 0.5)) if fx >= 0 else -int(math.floor(-fx + 0.5)), \
                 int(math.floor(fy + 0.5)) if fy >= 0 else -int(math.floor(-fy + 0.5))
        self.rem_x, self.rem_y = fx - ix, fy - iy
        if ix == 0 and iy == 0:
            return 0, 0, "deadzone"
        self.sent.append((now, ix, iy))
        return ix, iy, "move"
