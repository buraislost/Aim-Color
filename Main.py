"""
Main.py — BuraApp orchestrator
Reconstructed from Main.dll string/identifier analysis

Run: python Main.py
"""

import threading
import time
import logging
import os
import sys
import webbrowser
import json
import socket
from logging.handlers import RotatingFileHandler

from flask import Flask, request, redirect

from aimbot import aimbot, load_config, apply_cfg
from Driver import Mouse
from Capture import DirectXCapture, DXGICapture
from WebGui import get_html, _show_login_form
from user32 import get_async_key_state

# ---------- defaults ----------

_DEFAULT_CFG = {
    "color_to_track": "yellow",
    "aim_mode": "hold",
    "fovx": 50,
    "fovy": 50,
    "speedX": 3,
    "speedY": 3,
    "offset": 0,
    "tolerance": 0,
    "deadzone_x": 3,
    "deadzone_y": 3,
    "pixel_distance": 5,
    "base_delay": 0.05,
    "FOV_TRIGGER": 30,
    "magnet_strength": 0.5,
    "magnet_smooth": 3.0,
    "magnet_fov": 50,
    "magnet_version": "v1",
    "magnet_key": 0x02,
    "magnet_triggerbot": False,
    "trigger_hotkey": 0x14,   # Caps Lock
    "game_sens": 1.0,
    "anti_strafe": False,
    "humanize": True,
    "legitmode": False,
    "recoil_auto": False,
    "recoil_duration": 0,
    "recoil_x": 0,
    "recoil_y": 0,
}

colors = ["yellow", "red", "purple"]
aim_modes = ["hold", "toggle", "always"]
magnet_option = ["v1", "v2"]


# ---------- should_fire helpers ----------

def _should_fire(cfg: dict) -> bool:
    mode = cfg.get("aim_mode", "hold")
    hotkey = int(cfg.get("trigger_hotkey", 0x14))
    if mode == "always":
        return True
    if mode == "hold":
        return get_async_key_state(hotkey)
    # toggle handled in app state
    return False


def _trigger_should_fire(cfg: dict) -> bool:
    return _should_fire(cfg) and cfg.get("magnet_triggerbot", False)


def _magnet_should_fire(cfg: dict) -> bool:
    return get_async_key_state(int(cfg.get("magnet_key", 0x02)))


# ---------- app ----------

class BuraApp:
    def __init__(self):
        self.app = Flask(__name__)
        self.app.logger.setLevel(logging.WARNING)

        # Suppress Flask request logs
        log = logging.getLogger("werkzeug")
        log.setLevel(logging.ERROR)

        self.cfg = _DEFAULT_CFG.copy()
        saved = load_config()
        if saved:
            self.cfg.update(saved)

        self.mouse = Mouse()
        self.capture = None
        self.aim = None
        self.running = True
        self.port = self._get_free_port()

        self._setup_routes()

    def _get_free_port(self) -> int:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.bind(("", 0))
            return s.getsockname()[1]

    def _ensure_camera(self):
        if self.capture is None:
            try:
                self.capture = DXGICapture(fov=max(
                    int(self.cfg.get("fovx", 50)),
                    int(self.cfg.get("fovy", 50)),
                    int(self.cfg.get("magnet_fov", 50)),
                ) * 2)
            except Exception:
                self.capture = DirectXCapture(fov=max(
                    int(self.cfg.get("fovx", 50)),
                    int(self.cfg.get("fovy", 50)),
                ) * 2)

        if self.aim is None:
            self.aim = aimbot(self.mouse, self.capture, self.cfg)

    def _setup_routes(self):
        @self.app.route("/", methods=["GET"])
        def index():
            return get_html(self.cfg, colors, aim_modes, magnet_option)

        @self.app.route("/", methods=["POST"])
        def update_config():
            form = request.form.to_dict()

            # Checkboxes: present = True, absent = False
            for key in ["anti_strafe", "humanize", "legitmode", "recoil_auto", "magnet_triggerbot"]:
                form[key] = key in request.form

            # Convert numeric fields
            for key in ["fovx", "fovy", "speedX", "speedY", "offset", "tolerance",
                        "deadzone_x", "deadzone_y", "pixel_distance", "magnet_fov"]:
                if key in form:
                    try:
                        form[key] = int(float(form[key]))
                    except (ValueError, TypeError):
                        pass

            for key in ["base_delay", "magnet_strength", "magnet_smooth",
                        "game_sens", "recoil_x", "recoil_y", "recoil_duration"]:
                if key in form:
                    try:
                        form[key] = float(form[key])
                    except (ValueError, TypeError):
                        pass

            self.cfg.update(form)
            apply_cfg(self.cfg)

            if self.aim:
                self.aim.cfg = self.cfg

            return redirect("/")

        @self.app.route("/login", methods=["POST"])
        def login():
            # KeyAuth bypass — always succeed
            return redirect("/")

    def _heartbeat_loop(self):
        """Main aim loop — runs in a thread."""
        while self.running:
            try:
                self._ensure_camera()

                if _should_fire(self.cfg):
                    self.aim.trigger(self.cfg)

                if _magnet_should_fire(self.cfg):
                    self.aim.magnet(self.cfg)

                # Spinbot on separate key (optional)
                # self.aim.spinbot(self.cfg)

            except Exception as e:
                logging.debug(f"aim loop error: {e}")

            time.sleep(0.001)  # ~1000 Hz polling

    def auto_login(self):
        """Skip KeyAuth — patched out."""
        pass

    def _open_gui(self):
        time.sleep(1)
        webbrowser.open(f"http://127.0.0.1:{self.port}")

    def _create_tray_icon(self):
        try:
            import pystray
            from PIL import Image
            img = Image.new("RGB", (64, 64), "#e94560")
            icon = pystray.Icon(
                "Bura",
                img,
                menu=pystray.Menu(
                    pystray.MenuItem("Open", lambda: webbrowser.open(f"http://127.0.0.1:{self.port}")),
                    pystray.MenuItem("Exit", lambda: self._exit_app()),
                ),
            )
            icon.run()
        except ImportError:
            pass  # pystray not available

    def _exit_app(self):
        self.running = False
        if self.capture:
            self.capture.close()
        if self.mouse:
            self.mouse.cleanup()
        os._exit(0)

    def run(self):
        self.auto_login()

        # Start aim loop
        aim_thread = threading.Thread(target=self._heartbeat_loop, daemon=True)
        aim_thread.start()

        # Open browser
        threading.Thread(target=self._open_gui, daemon=True).start()

        # Tray icon in background
        threading.Thread(target=self._create_tray_icon, daemon=True).start()

        # Flask server
        self.app.run(host="127.0.0.1", port=self.port, debug=False, use_reloader=False)


if __name__ == "__main__":
    app = BuraApp()
    app.run()
