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


_MOUSE_NAMES = {0x01: "Mouse Left", 0x02: "Mouse Right", 0x04: "Mouse Middle",
                0x05: "Mouse X1", 0x06: "Mouse X2"}
_EXT_KEYS = {0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x2D, 0x2E, 0x6F}


def vk_name(vk) -> str:
    """Human-readable name of a virtual-key code (keyboard or mouse button)."""
    vk = int(vk)
    if vk == 0:
        return "None"
    if vk in _MOUSE_NAMES:
        return _MOUSE_NAMES[vk]
    sc = user32.MapVirtualKeyW(vk, 0)
    if sc:
        if vk in _EXT_KEYS:
            sc |= 0x100
        buf = ctypes.create_unicode_buffer(64)
        if user32.GetKeyNameTextW(sc << 16, buf, 64):
            return buf.value
    return f"VK 0x{vk:02X}"


def wait_for_key(timeout: float = 6.0):
    """Block until any key / mouse button (except left click) is pressed. -> vk or None."""
    import time
    end = time.time() + timeout
    # let go of whatever is held (the click that started the bind)
    while time.time() < end and any(user32.GetAsyncKeyState(v) & 0x8000 for v in range(1, 0xFF)):
        time.sleep(0.01)
    while time.time() < end:
        for v in range(2, 0xFF):
            if v in (0x10, 0x11, 0x12):     # generic Shift/Ctrl/Alt: L/R variants report too
                continue
            if user32.GetAsyncKeyState(v) & 0x8000:
                return v
        time.sleep(0.01)
    return None
