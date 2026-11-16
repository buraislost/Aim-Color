"""
Driver.py — MAKCU USB HID mouse driver
Reconstructed from Main.dll string/identifier analysis
"""

import serial
import serial.tools.list_ports
import time
import math
import random
import ctypes

from user32 import get_async_key_state


class MakcuMouse:
    """
    MAKCU (kmbox-style) serial mouse driver.
    Protocol: send ASCII commands terminated with \r
    Commands:
        km.move(dx,dy)   — relative mouse move
        km.left(1/0)     — left click press/release
        km.version()     — query firmware version
        km.buttons(1)    — query button state
    """

    SUPPORTED_VIDPIDS = [
        "1A86:55D3",
        "1A86:5523",
        "1A86:7523",
        "1A86:5740",
        "10C4:EA60",
    ]

    def __init__(self, baud: int = 128000, term: str = "\r"):
        self.ser = None
        self.port_name = None
        self.dev_name = "MAKCU"
        self.baud = int(baud)
        self.term = term
        self.found = False

        ports = serial.tools.list_ports.comports()
        for port in ports:
            hwid = port.hwid or ""
            desc = port.description or ""
            vidpid = ""
            name = port.device

            for vp in self.SUPPORTED_VIDPIDS:
                if vp.replace(":", "") in hwid.upper() or vp in hwid.upper():
                    vidpid = vp
                    break

            if vidpid:
                self.port_name = name
                self.found = True
                break

        if self.found:
            try:
                self.ser = serial.Serial(self.port_name, self.baud, timeout=1)
            except Exception:
                # port busy / unplugged mid-scan
                self.ser = None
                self.found = False
            else:
                # Query version to confirm connection
                try:
                    resp = self._send_cmd("km.version()")
                except Exception:
                    pass

    def _send_cmd(self, cmd: str) -> str:
        if self.ser and self.ser.is_open:
            self.ser.write((cmd + self.term).encode())
            time.sleep(0.005)
            resp = self.ser.read(self.ser.in_waiting or 64)
            return resp.decode(errors="ignore").strip()
        return ""

    def move(self, dx: int, dy: int):
        """Send relative mouse movement."""
        if self.ser and self.ser.is_open:
            dx = max(-127, min(127, int(dx)))
            dy = max(-127, min(127, int(dy)))
            self.ser.write(f"km.move({dx},{dy}){self.term}".encode())

    def slient_move(self, dx: int, dy: int):
        """Silent move — same as move but without clamping (typo preserved from original)."""
        if self.ser and self.ser.is_open:
            self.ser.write(f"km.move({int(dx)},{int(dy)}){self.term}".encode())

    def move_bezier(self, dx: int, dy: int, segments: int = 5):
        """
        Move mouse along a bezier curve from current position to (dx, dy).
        Uses a random control point for natural-looking movement.
        """
        if not self.ser or not self.ser.is_open:
            return

        ctrl_x = dx * random.uniform(0.2, 0.8)
        ctrl_y = dy * random.uniform(0.2, 0.8)

        prev_x, prev_y = 0.0, 0.0
        for i in range(1, segments + 1):
            t = i / segments
            # Quadratic bezier: B(t) = (1-t)^2 * P0 + 2(1-t)t * P1 + t^2 * P2
            bx = 2 * (1 - t) * t * ctrl_x + t * t * dx
            by = 2 * (1 - t) * t * ctrl_y + t * t * dy

            step_x = int(bx - prev_x)
            step_y = int(by - prev_y)
            prev_x = bx
            prev_y = by

            if step_x != 0 or step_y != 0:
                self.move(step_x, step_y)
                time.sleep(random.uniform(0.001, 0.004))

    def click(self):
        """Left click (press + release)."""
        if self.ser and self.ser.is_open:
            self.ser.write(f"km.left(1){self.term}".encode())
            time.sleep(random.uniform(0.03, 0.07))
            self.ser.write(f"km.left(0){self.term}".encode())

    def is_button_pressed(self) -> bool:
        """Query if a mouse button is currently pressed."""
        resp = self._send_cmd("km.buttons(1)")
        try:
            pressed = int(resp.strip()) if resp.strip().isdigit() else 0
            return pressed != 0
        except Exception:
            return False

    def cleanup(self):
        if self.ser and self.ser.is_open:
            self.ser.close()

    # ---- debug helpers ----
    @classmethod
    def list_ports(cls) -> list:
        out = []
        for p in serial.tools.list_ports.comports():
            hwid = (p.hwid or "").upper()
            match = any(vp in hwid or vp.replace(":", "") in hwid
                        for vp in cls.SUPPORTED_VIDPIDS)
            out.append({"device": p.device, "desc": p.description or "",
                        "hwid": p.hwid or "", "match": match})
        return out

    @staticmethod
    def probe(port: str,
              bauds=(115200, 4000000, 128000, 921600, 57600, 9600),
              terms=("\r\n", "\r")) -> list:
        """Try km.version() at several baud/terminator combos, return raw replies."""
        out = []
        for b in bauds:
            for t in terms:
                try:
                    with serial.Serial(port, b, timeout=0.3) as s:
                        s.reset_input_buffer()
                        s.write(("km.version()" + t).encode())
                        time.sleep(0.15)
                        reply = s.read(s.in_waiting or 64).decode(errors="replace")
                except Exception as e:
                    reply = f"ERR {type(e).__name__}"
                out.append({"baud": b, "term": t, "reply": reply})
        return out

    def test_move(self):
        """Test movement — small circle."""
        start = time.time()
        for i in range(36):
            angle = math.radians(i * 10)
            dx = int(3 * math.cos(angle))
            dy = int(3 * math.sin(angle))
            self.move(dx, dy)
            time.sleep(0.01)
        released = time.time() - start
        return released



# ---------------------------------------------------------------------------
# Direct input (software) — SendInput, no hardware needed
# ---------------------------------------------------------------------------

_ULONG_PTR = ctypes.c_size_t
_MOUSEEVENTF_MOVE = 0x0001
_MOUSEEVENTF_LEFTDOWN = 0x0002
_MOUSEEVENTF_LEFTUP = 0x0004


class _MOUSEINPUT(ctypes.Structure):
    _fields_ = [
        ("dx", ctypes.c_long),
        ("dy", ctypes.c_long),
        ("mouseData", ctypes.c_ulong),
        ("dwFlags", ctypes.c_ulong),
        ("time", ctypes.c_ulong),
        ("dwExtraInfo", _ULONG_PTR),
    ]


class _INPUT_UNION(ctypes.Union):
    _fields_ = [("mi", _MOUSEINPUT)]


class _INPUT(ctypes.Structure):
    _fields_ = [("type", ctypes.c_ulong), ("u", _INPUT_UNION)]


class SoftMouse:
    """Direct mouse input via SendInput. Same interface as MakcuMouse."""

    found = True
    port_name = "SendInput"

    def _send(self, flags: int, dx: int = 0, dy: int = 0):
        inp = _INPUT(type=0, u=_INPUT_UNION(mi=_MOUSEINPUT(dx, dy, 0, flags, 0, 0)))
        ctypes.windll.user32.SendInput(1, ctypes.byref(inp), ctypes.sizeof(inp))

    def move(self, dx: int, dy: int):
        dx = max(-127, min(127, int(dx)))
        dy = max(-127, min(127, int(dy)))
        if dx or dy:
            self._send(_MOUSEEVENTF_MOVE, dx, dy)

    def slient_move(self, dx: int, dy: int):
        if int(dx) or int(dy):
            self._send(_MOUSEEVENTF_MOVE, int(dx), int(dy))

    def move_bezier(self, dx: int, dy: int, segments: int = 5):
        ctrl_x = dx * random.uniform(0.2, 0.8)
        ctrl_y = dy * random.uniform(0.2, 0.8)
        prev_x, prev_y = 0.0, 0.0
        for i in range(1, segments + 1):
            t = i / segments
            bx = 2 * (1 - t) * t * ctrl_x + t * t * dx
            by = 2 * (1 - t) * t * ctrl_y + t * t * dy
            step_x = int(bx - prev_x)
            step_y = int(by - prev_y)
            prev_x, prev_y = bx, by
            if step_x != 0 or step_y != 0:
                self.move(step_x, step_y)
                time.sleep(random.uniform(0.001, 0.004))

    def click(self):
        self._send(_MOUSEEVENTF_LEFTDOWN)
        time.sleep(random.uniform(0.03, 0.07))
        self._send(_MOUSEEVENTF_LEFTUP)

    def is_button_pressed(self) -> bool:
        return get_async_key_state(0x01)

    def cleanup(self):
        pass

    def test_move(self):
        start = time.time()
        for i in range(36):
            angle = math.radians(i * 10)
            self.move(int(3 * math.cos(angle)), int(3 * math.sin(angle)))
            time.sleep(0.01)
        return time.time() - start


# ---------------------------------------------------------------------------
# Router — Main.py keeps using `Mouse()`; mode switches live from the WebGUI
# ---------------------------------------------------------------------------

class Mouse:
    MODES = ("makcu", "software")

    def __init__(self, baud: int = 128000, term: str = "\r"):
        self.baud = int(baud)
        self.term = term
        self.makcu = MakcuMouse(self.baud, self.term)
        self.soft = SoftMouse()
        self.mode = "makcu"

    @property
    def _d(self):
        return self.makcu if self.mode == "makcu" else self.soft

    def set_mode(self, mode: str):
        if mode in self.MODES:
            self.mode = mode

    def reconnect(self, baud=None, term=None) -> bool:
        """Re-scan serial ports and reopen the Makcu (optionally new baud/term)."""
        if baud is not None:
            self.baud = int(baud)
        if term is not None:
            self.term = term
        try:
            self.makcu.cleanup()
        except Exception:
            pass
        self.makcu = MakcuMouse(self.baud, self.term)
        return self.makcu.found

    def status(self) -> dict:
        return {
            "mode": self.mode,
            "makcu_found": bool(self.makcu.found),
            "makcu_port": self.makcu.port_name,
            "baud": self.makcu.baud,
        }

    def debug_info(self) -> dict:
        mk = self.makcu
        is_open = bool(mk.ser and mk.ser.is_open)
        reply = ""
        if is_open:
            try:
                reply = mk._send_cmd("km.version()")
            except Exception as e:
                reply = f"ERR {type(e).__name__}: {e}"
        return {"ports": MakcuMouse.list_ports(), "found": bool(mk.found),
                "port": mk.port_name, "baud": mk.baud, "term": mk.term,
                "open": is_open, "version": reply}

    def probe(self) -> dict:
        matched = [p["device"] for p in MakcuMouse.list_ports() if p["match"]]
        port = self.makcu.port_name or (matched[0] if matched else None)
        if not port:
            return {"port": None, "results": []}
        try:
            self.makcu.cleanup()
        except Exception:
            pass
        try:
            res = MakcuMouse.probe(port)
        finally:
            self.reconnect()
        return {"port": port, "results": res}

    def test_makcu(self) -> bool:
        """Visible 25px square, always through the Makcu regardless of mode."""
        mk = self.makcu
        if not (mk.ser and mk.ser.is_open):
            return False
        for dx, dy in ((25, 0), (0, 25), (-25, 0), (0, -25)):
            mk.move(dx, dy)
            time.sleep(0.15)
        return True

    # --- delegated API (same as MakcuMouse) ---
    def move(self, dx, dy):
        self._d.move(dx, dy)

    def slient_move(self, dx, dy):
        self._d.slient_move(dx, dy)

    def move_bezier(self, dx, dy, segments=5):
        self._d.move_bezier(dx, dy, segments)

    def click(self):
        self._d.click()

    def is_button_pressed(self):
        return self._d.is_button_pressed()

    def test_move(self):
        return self._d.test_move()

    def cleanup(self):
        for d in (self.makcu, self.soft):
            try:
                d.cleanup()
            except Exception:
                pass
