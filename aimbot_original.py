"""
aimbot.py — ORIGINAL reconstructed logic (pre-improvement)
Reconstructed from Main.dll identifier/variable analysis.

This is what the binary does. The improved version follows.
"""

import numpy as np
import cv2
import time
import ctypes
import json
import os
import random

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


# ---------- aimbot class ----------

class aimbot:
    """
    Original aim logic as reconstructed from the binary.
    Methods: trigger (main aimbot), flickbot, spinbot, magnet
    """

    def __init__(self, mouse, capture, cfg: dict):
        self.mouse = mouse
        self.capture = _CaptureWrapper(capture)
        self.cfg = cfg

        # Key state tracking
        self.key = 0
        self.tol = cfg.get("tolerance", 0)
        self.state = False      # toggle state
        self.key_state = False   # previous key state

    # ------------------------------------------------------------------ #
    #  METHOD 1: trigger — the main aimbot loop call                     #
    # ------------------------------------------------------------------ #
    def trigger(self, cfg: dict = None):
        """
        Main aimbot function. Called each frame from the main loop.
        Flow: capture → HSV convert → color mask → find pixels →
              pick closest to center → calc dx/dy → deadzone check →
              divide by speed → mouse.move
        """
        if cfg:
            self.cfg = cfg

        frame = self.capture.read()
        if frame is None:
            return

        tag = self.cfg.get("color_to_track", "yellow")
        tol = int(self.cfg.get("tolerance", 0))

        # Get color range
        raw = COLOR_RANGES.get(tag, COLOR_RANGES["yellow"])
        lower = raw["lower"]
        upper = raw["upper"]

        # Frame dimensions
        cy, cx = frame.shape[0] // 2, frame.shape[1] // 2
        halfX = int(self.cfg.get("fovx", 50))
        halfY = int(self.cfg.get("fovy", 50))

        # Crop ROI around center
        roi = frame[max(0, cy - halfY):cy + halfY, max(0, cx - halfX):cx + halfX]

        # Convert to HSV and mask
        hsv = cv2.cvtColor(roi, cv2.COLOR_BGR2HSV)
        mask = cv2.inRange(hsv, lower, upper)

        # Find matching pixels
        ys, xs = np.nonzero(mask)

        if len(xs) == 0:
            return

        # Pick topmost pixel (min_y) — aims at head
        min_y = np.min(ys)
        head_mask = ys == min_y
        target_x = int(np.mean(xs[head_mask]))
        target_y = int(min_y)

        # Center of ROI
        center_x = halfX
        center_y = halfY

        # Offset (vertical aim offset, e.g. aim higher for headshot)
        offset = int(self.cfg.get("offset", 0))
        target_y += offset

        # Calculate delta
        dx = target_x - center_x
        dy = target_y - center_y

        # Deadzone check
        deadzone_x = int(self.cfg.get("deadzone_x", 3))
        deadzone_y = int(self.cfg.get("deadzone_y", 3))

        if abs(dx) < deadzone_x and abs(dy) < deadzone_y:
            return

        # Apply speed divisor
        speedX = max(1, float(self.cfg.get("speedX", 3)))
        speedY = max(1, float(self.cfg.get("speedY", 3)))

        step_x = int(dx / speedX)
        step_y = int(dy / speedY)

        # Anti-strafe: reduce horizontal movement
        if self.cfg.get("anti_strafe", False):
            step_x = int(step_x * 0.7)

        # Humanize: add small random noise
        if self.cfg.get("humanize", False):
            step_x += random.randint(-1, 1)
            step_y += random.randint(-1, 1)

        # Legitmode: cap maximum step size
        if self.cfg.get("legitmode", False):
            max_step = 5
            step_x = max(-max_step, min(max_step, step_x))
            step_y = max(-max_step, min(max_step, step_y))

        if step_x != 0 or step_y != 0:
            self.mouse.move(step_x, step_y)

        # Recoil compensation
        if self.cfg.get("recoil_auto", False):
            recoil_x = float(self.cfg.get("recoil_x", 0))
            recoil_y = float(self.cfg.get("recoil_y", 0))
            if recoil_x != 0 or recoil_y != 0:
                self.mouse.move(int(recoil_x), int(recoil_y))

        # Triggerbot (auto-fire when on target)
        if self.cfg.get("magnet_triggerbot", False):
            if abs(dx) < int(self.cfg.get("pixel_distance", 5)):
                time.sleep(float(self.cfg.get("base_delay", 0.05)))
                self.mouse.click()

        # Config hot-reload check
        new_cfg = load_config()
        if new_cfg:
            self.cfg = new_cfg

    # ------------------------------------------------------------------ #
    #  METHOD 2: flickbot                                                #
    # ------------------------------------------------------------------ #
    def flickbot(self, cfg: dict = None):
        """
        Flick-style aim. Detects target, calculates full displacement,
        sends as one fast move (or bezier curve).
        """
        if cfg:
            self.cfg = cfg

        frame = self.capture.read()
        if frame is None:
            return

        right_state = ctypes.windll.user32.GetAsyncKeyState(
            int(self.cfg.get("flick_key", 0x02))  # right mouse button
        )
        if not (right_state & 0x8000):
            return

        now = time.time()

        tag = self.cfg.get("color_to_track", "yellow")
        raw = COLOR_RANGES.get(tag, COLOR_RANGES["yellow"])

        cx, cy = frame.shape[1] // 2, frame.shape[0] // 2
        half = int(self.cfg.get("flick_fov", 60))

        roi = frame[max(0, cy - half):cy + half, max(0, cx - half):cx + half]
        hsv = cv2.cvtColor(roi, cv2.COLOR_BGR2HSV)
        mask = cv2.inRange(hsv, raw["lower"], raw["upper"])

        ys, xs = np.nonzero(mask)
        if len(xs) == 0:
            return

        min_y = np.min(ys)
        target_x = int(np.mean(xs[ys == min_y]))
        target_y = int(min_y)

        center_x = half
        center_y = half

        dx = target_x - center_x
        dy = target_y - center_y

        move_x = int(dx / max(1, float(self.cfg.get("flick_speedx", 2))))
        move_y = int(dy / max(1, float(self.cfg.get("flick_speedy", 2))))

        # Send as bezier for smoother flick
        data = self.cfg.get("flick_delay", 0.01)
        self.mouse.move_bezier(move_x, move_y, segments=5)
        time.sleep(float(data))

    # ------------------------------------------------------------------ #
    #  METHOD 3: spinbot                                                 #
    # ------------------------------------------------------------------ #
    def spinbot(self, cfg: dict = None):
        """
        Spinbot — continuous rotation. Mostly for fun/anti-aim.
        """
        if cfg:
            self.cfg = cfg

        frame = self.capture.read()
        if frame is None:
            return

        cx, cy = frame.shape[1] // 2, frame.shape[0] // 2

        tag = self.cfg.get("color_to_track", "yellow")
        raw = COLOR_RANGES.get(tag, COLOR_RANGES["yellow"])

        hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
        mask = cv2.inRange(hsv, raw["lower"], raw["upper"])
        cropped = mask

        ys, xs = np.nonzero(cropped)
        if len(xs) > 0:
            dx = int(np.mean(xs)) - cx
            move_x = int(dx * float(self.cfg.get("game_sens", 1.0)))
            old_dx = move_x
            self.mouse.move(move_x, 0)

    # ------------------------------------------------------------------ #
    #  METHOD 4: magnet — sticky aim / magnet aim                        #
    # ------------------------------------------------------------------ #
    def magnet(self, cfg: dict = None):
        """
        Magnet aim — smoothly pulls crosshair toward target.
        Uses configurable strength and smooth factor.
        """
        if cfg:
            self.cfg = cfg

        key = int(self.cfg.get("magnet_key", 0x02))
        right_state = ctypes.windll.user32.GetAsyncKeyState(key)
        if not (right_state & 0x8000):
            return

        frame = self.capture.read()
        if frame is None:
            return

        HEIGHT, WIDTH = frame.shape[0], frame.shape[1]

        base_half = int(self.cfg.get("magnet_fov", 50))
        cx = WIDTH // 2
        cy = HEIGHT // 2

        cropped_frame = frame[
            max(0, cy - base_half):cy + base_half,
            max(0, cx - base_half):cx + base_half
        ]

        tag = self.cfg.get("color_to_track", "yellow")
        raw = COLOR_RANGES.get(tag, COLOR_RANGES["yellow"])

        hsv = cv2.cvtColor(cropped_frame, cv2.COLOR_BGR2HSV)
        mask = cv2.inRange(hsv, raw["lower"], raw["upper"])

        ys, xs = np.nonzero(mask)
        if len(xs) == 0:
            return

        # Find closest target to center
        center_x = base_half
        center_y = base_half

        target_x = xs
        dx = target_x - center_x

        distances = np.sqrt((xs - center_x) ** 2 + (ys - center_y) ** 2)
        closest = np.argmin(distances)

        target_dx = int(xs[closest] - center_x)
        target_dy = int(ys[closest] - center_y)

        # Apply magnet strength
        strength = float(self.cfg.get("magnet_strength", 0.5))
        smooth = float(self.cfg.get("magnet_smooth", 3.0))

        move_x = int(target_dx * strength / smooth)
        move_y = int(target_dy * strength / smooth)

        if move_x != 0 or move_y != 0:
            self.mouse.move(move_x, move_y)
