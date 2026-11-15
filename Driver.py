"""
Driver.py — MAKCU USB HID mouse driver
Reconstructed from Main.dll string/identifier analysis
"""

import serial
import serial.tools.list_ports
import time
import math
import random


class Mouse:
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

    def __init__(self):
        self.ser = None
        self.port_name = None
        self.dev_name = "MAKCU"
        self.baud = 128000
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
            self.ser = serial.Serial(self.port_name, self.baud, timeout=1)
            # Query version to confirm connection
            try:
                resp = self._send_cmd("km.version()")
            except Exception:
                pass

    def _send_cmd(self, cmd: str) -> str:
        if self.ser and self.ser.is_open:
            self.ser.write((cmd + "\r").encode())
            time.sleep(0.005)
            resp = self.ser.read(self.ser.in_waiting or 64)
            return resp.decode(errors="ignore").strip()
        return ""

    def move(self, dx: int, dy: int):
        """Send relative mouse movement."""
        if self.ser and self.ser.is_open:
            dx = max(-127, min(127, int(dx)))
            dy = max(-127, min(127, int(dy)))
            self.ser.write(f"km.move({dx},{dy})\r".encode())

    def slient_move(self, dx: int, dy: int):
        """Silent move — same as move but without clamping (typo preserved from original)."""
        if self.ser and self.ser.is_open:
            self.ser.write(f"km.move({int(dx)},{int(dy)})\r".encode())

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
            self.ser.write(b"km.left(1)\r")
            time.sleep(random.uniform(0.03, 0.07))
            self.ser.write(b"km.left(0)\r")

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
