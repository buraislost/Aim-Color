"""
Capture.py — DirectX screen capture
Reconstructed from Main.dll string/identifier analysis
"""

import ctypes
from ctypes import wintypes, c_void_p, byref, windll
import numpy as np

# Without this, GetSystemMetrics returns scaled (virtual) sizes at 125%/150%
# display scaling and the capture region lands off-center.
try:
    windll.user32.SetProcessDPIAware()
except Exception:
    pass


# ---------- crosshair center: game window, not screen ----------
# Windowed (non-maximized / offset) games have their crosshair at the client-area
# center of the window, which is NOT the screen center. Track the window.

import time as _time

_WNDENUMPROC = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)


def _find_window(sub: str):
    u = windll.user32
    sub = sub.lower()
    found = []

    def cb(hwnd, _):
        if u.IsWindowVisible(hwnd) and not u.IsIconic(hwnd):
            n = u.GetWindowTextLengthW(hwnd)
            if n:
                buf = ctypes.create_unicode_buffer(n + 1)
                u.GetWindowTextW(hwnd, buf, n + 1)
                if sub in buf.value.lower():
                    found.append(hwnd)
                    return False
        return True

    u.EnumWindows(_WNDENUMPROC(cb), 0)
    return found[0] if found else None


def get_center():
    """((cx, cy), src) in primary-screen coords. src = 'window' | 'screen'."""
    u = windll.user32
    sw, sh = u.GetSystemMetrics(0), u.GetSystemMetrics(1)
    try:
        from aimbot import load_key
        title = str(load_key("capture_window", "VALORANT"))
    except Exception:
        title = "VALORANT"
    if title:
        try:
            hwnd = _find_window(title)
            if hwnd:
                rc = wintypes.RECT()
                pt = wintypes.POINT(0, 0)
                u.GetClientRect(hwnd, byref(rc))
                u.ClientToScreen(hwnd, byref(pt))
                if rc.right > 0 and rc.bottom > 0:
                    cx, cy = pt.x + rc.right // 2, pt.y + rc.bottom // 2
                    if 0 <= cx < sw and 0 <= cy < sh:
                        return (cx, cy), "window"
        except Exception:
            pass
    return (sw // 2, sh // 2), "screen"


class DirectXCapture:
    """
    GDI capture of the FOV box around the crosshair (the game window center).
    Shares the cached-DC grabber below, which declares ctypes argtypes: the old
    inline version passed a 64-bit buffer pointer as a plain int
    ("ArgumentError: argument 5: OverflowError: int too long to convert").
    """

    __slots__ = ("_screen_w", "_screen_h", "_fov", "_region",
                 "center", "center_src", "_c_t")

    def __init__(self, fov=100, **_ignored):
        u = windll.user32
        self._screen_w = u.GetSystemMetrics(0)
        self._screen_h = u.GetSystemMetrics(1)
        self._fov = int(fov)
        self.center = (self._screen_w // 2, self._screen_h // 2)
        self.center_src = "screen"
        self._c_t = 0.0
        self._update_region()

    def _update_region(self):
        half = self._fov // 2
        (cx, cy), src = get_center()
        self.center, self.center_src = (cx, cy), src
        x = max(0, min(self._screen_w - self._fov, cx - half))
        y = max(0, min(self._screen_h - self._fov, cy - half))
        self._region = (x, y, self._fov, self._fov)

    def set_fov(self, fov: int):
        self._fov = int(fov)
        self._update_region()

    def capture_screen(self) -> np.ndarray:
        """FOV box around the crosshair as a BGR copy."""
        now = _time.perf_counter()
        if now - self._c_t > 0.5:      # window may move / resize
            self._c_t = now
            self._update_region()
        x, y, w, h = self._region
        return grab_region(x, y, w, h)

    def recreate(self):
        """Resolution / FOV change: recompute the region."""
        self._screen_w = windll.user32.GetSystemMetrics(0)
        self._screen_h = windll.user32.GetSystemMetrics(1)
        self._update_region()

    def close(self):
        pass

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()


class DXGICapture:
    """
    bettercam DXGI desktop duplication, polled synchronously from the aim thread.

    bettercam's own capture thread is NOT used: on "access lost" (display mode change,
    game focus, UAC, hybrid-GPU laptops) the library rebuilds its frame buffer at full
    screen size while the thread keeps grabbing the small FOV box ->
    "could not broadcast (100,100,3) into (1080,1920,3)", the thread dies, and
    get_latest_frame() then serves uninitialised (black) memory or blocks forever.
    grab() is non-blocking (AcquireNextFrame timeout 0) and None means "no new frame".
    """

    def __init__(self, fov=100):
        import bettercam
        self._fov = int(fov)
        self.center = (0, 0)
        self.center_src = "screen"
        self._c_t = 0.0
        self._t_born = _time.perf_counter()
        self._last = None
        self._black_t = None
        self._cam = bettercam.create(output_color="BGR")
        self._region = self._get_region()
        # the first AcquireNextFrame often times out: poll briefly for a real frame
        t0 = _time.perf_counter()
        while self._last is None and _time.perf_counter() - t0 < 0.5:
            if self._pull() is None:
                _time.sleep(0.01)
        if self._last is not None and self._last.max() == 0:
            self.close()
            raise RuntimeError("DXGI frames are black (game on another GPU / output?)")

    def _get_region(self):
        user32 = windll.user32
        sw = user32.GetSystemMetrics(0)
        sh = user32.GetSystemMetrics(1)
        half = self._fov // 2
        (cx, cy), src = get_center()
        self.center, self.center_src = (cx, cy), src
        l = max(0, min(sw - self._fov, cx - half))
        t = max(0, min(sh - self._fov, cy - half))
        return (l, t, l + self._fov, t + self._fov)

    def _pull(self):
        f = self._cam.grab(self._region)       # None = nothing new on screen yet
        if f is not None:
            self._last = f
            if f.max() == 0:
                if self._black_t is None:
                    self._black_t = _time.perf_counter()
            else:
                self._black_t = None
        return f

    def set_fov(self, fov: int):
        self._fov = int(fov)
        self._c_t = 0.0                        # recompute the region on the next capture

    def capture_screen(self) -> np.ndarray:
        now = _time.perf_counter()
        if now - self._c_t > 0.5:              # window may move / resize / FOV changed
            self._c_t = now
            r = self._get_region()
            if r != self._region:
                self._region = r
                if self._last is not None and self._last.shape[:2] != (r[3] - r[1], r[2] - r[0]):
                    self._last = None
        self._pull()
        if self._last is None:
            return np.zeros((self._fov, self._fov, 3), dtype=np.uint8)
        return self._last

    def health(self) -> str:
        """'' while frames look real, otherwise why Main should fall back to GDI."""
        now = _time.perf_counter()
        if self._last is None:
            return "DXGI gave no frames" if now - self._t_born > 3.0 else ""
        if self._black_t is not None and now - self._black_t > 3.0:
            return "DXGI frames are black"
        return ""

    def close(self):
        cam, self._cam = self._cam, None
        if cam is not None:
            del cam                            # __del__ releases the COM objects once
            import gc
            gc.collect()

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()


# ---------- full-screen debug view (GDI, independent of the aim capture) ----------

class _BMIH(ctypes.Structure):
    _fields_ = [("biSize", wintypes.DWORD), ("biWidth", wintypes.LONG),
                ("biHeight", wintypes.LONG), ("biPlanes", wintypes.WORD),
                ("biBitCount", wintypes.WORD), ("biCompression", wintypes.DWORD),
                ("biSizeImage", wintypes.DWORD), ("biXPelsPerMeter", wintypes.LONG),
                ("biYPelsPerMeter", wintypes.LONG), ("biClrUsed", wintypes.DWORD),
                ("biClrImportant", wintypes.DWORD)]


class _ScreenGrab:
    """Reusable GDI full-screen grabber (DC/bitmap/buffer allocated once)."""

    def __init__(self):
        import threading
        self._lock = threading.Lock()
        u = ctypes.WinDLL("user32")
        g = ctypes.WinDLL("gdi32")
        u.GetDC.restype = c_void_p
        u.GetDC.argtypes = [c_void_p]
        g.CreateCompatibleDC.restype = c_void_p
        g.CreateCompatibleDC.argtypes = [c_void_p]
        g.CreateCompatibleBitmap.restype = c_void_p
        g.CreateCompatibleBitmap.argtypes = [c_void_p, ctypes.c_int, ctypes.c_int]
        g.SelectObject.restype = c_void_p
        g.SelectObject.argtypes = [c_void_p, c_void_p]
        g.BitBlt.argtypes = [c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int,
                             c_void_p, ctypes.c_int, ctypes.c_int, wintypes.DWORD]
        g.GetDIBits.argtypes = [c_void_p, c_void_p, wintypes.UINT, wintypes.UINT,
                                c_void_p, c_void_p, wintypes.UINT]
        self.u, self.g = u, g
        self.sw, self.sh = u.GetSystemMetrics(0), u.GetSystemMetrics(1)
        self.hdc = u.GetDC(None)
        self.mem = g.CreateCompatibleDC(self.hdc)
        self.bmp = g.CreateCompatibleBitmap(self.hdc, self.sw, self.sh)
        g.SelectObject(self.mem, self.bmp)
        self.bi = _BMIH(40, self.sw, -self.sh, 1, 32, 0, 0, 0, 0, 0, 0)
        self.buf = np.empty((self.sh, self.sw, 4), dtype=np.uint8)
        self._regions = {}

    def grab_region(self, x, y, w, h):
        with self._lock:
            ent = self._regions.get((w, h))
            if ent is None:
                mem = self.g.CreateCompatibleDC(self.hdc)
                bmp = self.g.CreateCompatibleBitmap(self.hdc, w, h)
                self.g.SelectObject(mem, bmp)
                ent = (mem, bmp, np.empty((h, w, 4), dtype=np.uint8), _BMIH(40, w, -h, 1, 32, 0, 0, 0, 0, 0, 0))
                self._regions[(w, h)] = ent
            mem, bmp, buf, bi = ent
            self.g.BitBlt(mem, 0, 0, w, h, self.hdc, x, y, 0x00CC0020)
            self.g.GetDIBits(mem, bmp, 0, h, buf.ctypes.data, byref(bi), 0)
            return np.ascontiguousarray(buf[:, :, :3])

    def grab(self):
        with self._lock:
            self.g.BitBlt(self.mem, 0, 0, self.sw, self.sh, self.hdc, 0, 0, 0x00CC0020 | 0x40000000)
            self.g.GetDIBits(self.mem, self.bmp, 0, self.sh, self.buf.ctypes.data, byref(self.bi), 0)
            return np.ascontiguousarray(self.buf[:, :, :3])


_SG = None


def _get_sg():
    global _SG
    u0 = windll.user32
    if _SG is None or (_SG.sw, _SG.sh) != (u0.GetSystemMetrics(0), u0.GetSystemMetrics(1)):
        _SG = _ScreenGrab()
    return _SG


def screen_size():
    u0 = windll.user32
    return u0.GetSystemMetrics(0), u0.GetSystemMetrics(1)


def grab_region(x: int, y: int, w: int, h: int):
    """BGR copy of an arbitrary screen rectangle (GDI, cached buffers per size)."""
    return _get_sg().grab_region(int(x), int(y), int(w), int(h))


def grab_screen_jpg(cfg: dict, center=None, src="screen", max_w=1280) -> bytes:
    """Whole primary screen (GDI) + FOV box, magnet box, crosshair and capture center."""
    import cv2
    global _SG
    u0 = windll.user32
    if _SG is None or (_SG.sw, _SG.sh) != (u0.GetSystemMetrics(0), u0.GetSystemMetrics(1)):
        _SG = _ScreenGrab()
    sw, sh = _SG.sw, _SG.sh
    img = _SG.grab()
    cx, cy = center if center else (sw // 2, sh // 2)
    hx, hy = int(cfg.get("fovx", 50)), int(cfg.get("fovy", 50))
    mf = int(cfg.get("magnet_fov", 50))
    th = max(2, sw // 640)
    cv2.rectangle(img, (cx - mf, cy - mf), (cx + mf, cy + mf), (255, 160, 60), th)   # magnet (BGR)
    cv2.rectangle(img, (cx - hx, cy - hy), (cx + hx, cy + hy), (60, 255, 60), th)    # aim FOV
    L = max(12, sw // 160)
    cv2.line(img, (cx - L, cy), (cx + L, cy), (255, 255, 255), th)
    cv2.line(img, (cx, cy - L), (cx, cy + L), (255, 255, 255), th)
    if src != "screen":   # also mark real screen center so the offset is visible
        cv2.drawMarker(img, (sw // 2, sh // 2), (0, 0, 255), cv2.MARKER_TILTED_CROSS, L * 2, th)
    if sw > max_w:
        s = max_w / float(sw)
        img = cv2.resize(img, (max_w, int(sh * s)), interpolation=cv2.INTER_AREA)
    ok, enc = cv2.imencode(".jpg", img, [cv2.IMWRITE_JPEG_QUALITY, 70])
    return enc.tobytes() if ok else b""
