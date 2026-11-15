"""
user32.py — Win32 API bindings
Reconstructed from Main.dll identifiers
"""

import ctypes
from ctypes import wintypes, windll

user32 = windll.user32
kernel32 = windll.kernel32


def get_async_key_state(vk: int) -> bool:
    """Check if a virtual key is currently pressed."""
    return bool(user32.GetAsyncKeyState(vk) & 0x8000)


def get_cursor_pos() -> tuple:
    """Get current cursor position."""
    pt = wintypes.POINT()
    user32.GetCursorPos(ctypes.byref(pt))
    return (pt.x, pt.y)


def get_screen_size() -> tuple:
    """Get screen resolution."""
    w = user32.GetSystemMetrics(0)
    h = user32.GetSystemMetrics(1)
    return (w, h)


def get_foreground_window():
    return user32.GetForegroundWindow()


def get_window_text(hwnd) -> str:
    buf = ctypes.create_unicode_buffer(256)
    user32.GetWindowTextW(hwnd, buf, 256)
    return buf.value
