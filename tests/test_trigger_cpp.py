"""End-to-end: aimbot.trigger() with the C++-port engine against a virtual world. Run: python tests/test_trigger_cpp.py"""
import os, sys, types
import numpy as np, cv2
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
for n in ("win32api", "win32con"):
    sys.modules.setdefault(n, types.SimpleNamespace(GetAsyncKeyState=lambda k: 0))
import aimbot
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from test_cbcore import ring

class Clock:
    t = 100.0
    def perf_counter(self): return self.t
    def time(self): return self.t
    def sleep(self, s): self.t += s

class World:
    """Bot head at (head_x, head_y) on a 400x400 screen; crosshair fixed at the centre. A mouse count
    moves the view `lag` frames later (1 count = 1 px)."""
    def __init__(self, ox, oy, lag=2):
        self.o = [float(ox), float(oy)]; self.pipe = [(0, 0)] * lag; self.moves = 0
    def capture_screen(self):
        f = np.zeros((400, 400, 3), np.uint8)
        hx, hy = int(round(200 + self.o[0])), int(round(200 + self.o[1]))
        ring(f, hx, hy - 8, 14, 16)
        return f
    def close(self): pass
    def move(self, dx, dy): self.moves += 1; self.pipe.append((dx, dy))
    def click(self): pass
    def tick(self):
        if not self.pipe: return
        dx, dy = self.pipe.pop(0); self.o[0] -= dx; self.o[1] -= dy
        self.pipe.append((0, 0))

def run(ox, oy, engine="cpp", seconds=2.5):
    clk = Clock(); aimbot.time = clk
    w = World(ox, oy)
    cfg = {"aim_engine": engine, "fovx": 100, "fovy": 100, "color_to_track": "yellow"}
    a = aimbot.aimbot(w, w, cfg)
    while clk.t < 100.0 + seconds:
        a.trigger(cfg); w.tick(); clk.t += 1 / 120.0
    run.last = tuple(w.o)
    return float(np.hypot(*w.o)), w.moves, a

def test_aims_at_head_and_stops():
    for st in [(45, -35), (-60, 30), (0, 60)]:
        err, moves, a = run(*st)
        ox, oy = run.last
        assert abs(ox) <= 8 and abs(oy) <= 8, (st, run.last)   # crosshair inside the 16 px head
        assert moves > 3
        assert any(l[1] == "hold" for l in a.log), "must reach the hold state on the head"

def test_no_target_does_nothing():
    clk = Clock(); aimbot.time = clk
    w = World(500, 500)                            # head outside the FOV box
    cfg = {"aim_engine": "cpp", "fovx": 100, "fovy": 100}
    a = aimbot.aimbot(w, w, cfg)
    for _ in range(60): a.trigger(cfg); w.tick(); clk.t += 1 / 120.0
    assert w.moves == 0

if __name__ == "__main__":
    fails = 0
    for k, v in sorted(globals().items()):
        if k.startswith("test_"):
            try: v(); print("PASS", k)
            except AssertionError as e: fails += 1; print("FAIL", k, e)
    sys.exit(1 if fails else 0)
