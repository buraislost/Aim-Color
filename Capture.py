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


class DirectXCapture:
    """
    GDI-based screen capture with triple buffering.
    Captures a region of the screen centered on the crosshair.
    """

    __slots__ = (
        "_hdc_src",
        "_buffers",
        "_cur",
        "_screen_w",
        "_screen_h",
        "_fov",
        "_region",
        "_blit_flags",
    )

    def __init__(self, fov=100, output_color="BGR", include_layered=True, triple_buffer=True):
        user32 = windll.user32
        gdi32 = windll.gdi32

        self._screen_w = user32.GetSystemMetrics(0)
        self._screen_h = user32.GetSystemMetrics(1)
        self._fov = fov
        self._cur = 0
        self._blit_flags = 0x00CC0020 | (0x40000000 if include_layered else 0)  # SRCCOPY | CAPTUREBLT

        self._update_region()

        # Get screen DC
        self._hdc_src = user32.GetDC(0)

        # Create compatible DCs and bitmaps for triple buffering
        num_buffers = 3 if triple_buffer else 1
        self._buffers = []
        for _ in range(num_buffers):
            hdc = gdi32.CreateCompatibleDC(self._hdc_src)
            hbm = gdi32.CreateCompatibleBitmap(self._hdc_src, self._fov, self._fov)
            gdi32.SelectObject(hdc, hbm)
            self._buffers.append((hdc, hbm))

    def _update_region(self):
        half = self._fov // 2
        cx = self._screen_w // 2
        cy = self._screen_h // 2
        self._region = (cx - half, cy - half, self._fov, self._fov)

    def set_fov(self, fov: int):
        self._fov = fov
        self._update_region()

    def capture_screen(self) -> np.ndarray:
        """Capture the FOV region around screen center. Returns BGR numpy array."""
        gdi32 = windll.gdi32

        idx = self._cur % len(self._buffers)
        self._cur += 1
        hdc_dst, hbm = self._buffers[idx]

        x, y, w, h = self._region

        # BitBlt from screen
        gdi32.BitBlt(hdc_dst, 0, 0, w, h, self._hdc_src, x, y, self._blit_flags)

        # Extract pixels via GetBitmapBits
        bmi = ctypes.create_string_buffer(40)
        ctypes.memmove(bmi, b'\x28\x00\x00\x00', 4)  # biSize
        ctypes.memmove(ctypes.addressof(ctypes.c_char.from_buffer(bmi, 4)),
                       w.to_bytes(4, 'little'), 4)
        ctypes.memmove(ctypes.addressof(ctypes.c_char.from_buffer(bmi, 8)),
                       (-(h)).to_bytes(4, 'little', signed=True), 4)
        ctypes.memmove(ctypes.addressof(ctypes.c_char.from_buffer(bmi, 12)),
                       b'\x01\x00', 2)  # biPlanes
        ctypes.memmove(ctypes.addressof(ctypes.c_char.from_buffer(bmi, 14)),
                       b'\x20\x00', 2)  # biBitCount = 32

        buf = np.empty((h, w, 4), dtype=np.uint8)
        gdi32.GetDIBits(hdc_dst, hbm, 0, h, buf.ctypes.data, bmi, 0)

        return buf[:, :, :3]  # Drop alpha, return BGR

    def recreate(self):
        """Recreate capture resources (e.g. after resolution change)."""
        self.close()
        self.__init__(fov=self._fov)

    def close(self):
        gdi32 = windll.gdi32
        user32 = windll.user32
        for hdc, hbm in self._buffers:
            gdi32.DeleteDC(hdc)
            gdi32.DeleteObject(hbm)
        self._buffers.clear()
        if self._hdc_src:
            user32.ReleaseDC(0, self._hdc_src)
            self._hdc_src = None

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()


class DXGICapture:
    """Wrapper using bettercam for DXGI desktop duplication capture."""

    def __init__(self, fov=100):
        import bettercam
        self._fov = fov
        self._cam = bettercam.create(output_color="BGR")
        self._cam.start(region=self._get_region(), video_mode=True, target_fps=240)

    def _get_region(self):
        user32 = windll.user32
        sw = user32.GetSystemMetrics(0)
        sh = user32.GetSystemMetrics(1)
        half = self._fov // 2
        cx, cy = sw // 2, sh // 2
        return (cx - half, cy - half, cx + half, cy + half)

    def set_fov(self, fov: int):
        self._fov = fov
        self._cam.stop()
        self._cam.start(region=self._get_region(), video_mode=True, target_fps=240)

    def capture_screen(self) -> np.ndarray:
        frame = self._cam.get_latest_frame()
        if frame is None:
            return np.zeros((self._fov, self._fov, 3), dtype=np.uint8)
        return frame

    def close(self):
        self._cam.stop()

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()
