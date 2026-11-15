"""
WebGui.py — Flask WebGUI HTML template
Compact tabbed config panel — Bura
"""


def _show_login_form() -> str:
    return """
    <html><body style="background:#111;color:#eee;font-family:system-ui,sans-serif;text-align:center;padding-top:100px">
    <h2 style="color:#7c5cfc">Bura</h2>
    <form method="post" action="/login">
        <input name="key" placeholder="License Key" style="padding:10px;width:280px;background:#1a1a1a;color:#eee;border:1px solid #333;border-radius:6px"><br><br>
        <button type="submit" style="padding:10px 32px;background:#7c5cfc;color:#fff;border:none;border-radius:6px;cursor:pointer;font-weight:600">Login</button>
    </form>
    </body></html>
    """


def get_html(cfg: dict, colors: list, aim_modes: list, magnet_option: list) -> str:
    """Generate the main config page HTML with compact tabbed layout."""

    def _s(name, label, val, min_v=0, max_v=100, step=1):
        """Compact inline slider: label [---o---] value"""
        return f'<div class="s"><span class="l">{label}</span><input type="range" name="{name}" min="{min_v}" max="{max_v}" step="{step}" value="{val}" oninput="this.nextElementSibling.textContent=this.value"><span class="v" id="v_{name}">{val}</span></div>'

    def _sel(name, label, options, selected):
        opts = "".join(
            f'<option value="{o}" {"selected" if o == selected else ""}>{o}</option>'
            for o in options
        )
        return f'<div class="s"><span class="l">{label}</span><select name="{name}">{opts}</select></div>'

    def _t(name, label, checked):
        chk = "checked" if checked else ""
        return f'<div class="s"><span class="l">{label}</span><label class="sw"><input type="checkbox" name="{name}" {chk}><span class="dot"></span></label></div>'

    # --- Tab: Aimbot ---
    tab_aim = "".join([
        _sel("color_to_track", "Color", colors, cfg.get("color_to_track", "yellow")),
        _sel("aim_mode", "Mode", aim_modes, cfg.get("aim_mode", "hold")),
        _s("fovx", "FOV X", cfg.get("fovx", 50), 10, 500),
        _s("fovy", "FOV Y", cfg.get("fovy", 50), 10, 500),
        _s("speedX", "Speed X", cfg.get("speedX", 3), 1, 30),
        _s("speedY", "Speed Y", cfg.get("speedY", 3), 1, 30),
        _s("deadzone_x", "Dead X", cfg.get("deadzone_x", 3), 0, 20),
        _s("deadzone_y", "Dead Y", cfg.get("deadzone_y", 3), 0, 20),
        _s("offset", "Y Offset", cfg.get("offset", 0), -50, 50),
        _s("tolerance", "Tolerance", cfg.get("tolerance", 0), 0, 30),
        _s("game_sens", "Sensitivity", cfg.get("game_sens", 1.0), 0.1, 5.0, 0.1),
        '<div class="tg">',
        _t("anti_strafe", "Anti-Strafe", cfg.get("anti_strafe", False)),
        _t("humanize", "Humanize", cfg.get("humanize", False)),
        _t("legitmode", "Legit", cfg.get("legitmode", False)),
        '</div>',
    ])

    # --- Tab: Triggerbot ---
    tab_trigger = "".join([
        _s("pixel_distance", "Distance", cfg.get("pixel_distance", 5), 1, 30),
        _s("base_delay", "Delay (s)", cfg.get("base_delay", 0.05), 0, 0.5, 0.01),
        _t("magnet_triggerbot", "Enable", cfg.get("magnet_triggerbot", False)),
    ])

    # --- Tab: Magnet ---
    tab_magnet = "".join([
        _sel("magnet_version", "Mode", magnet_option, cfg.get("magnet_version", "v1")),
        _s("magnet_strength", "Strength", cfg.get("magnet_strength", 0.5), 0.1, 2.0, 0.1),
        _s("magnet_smooth", "Smooth", cfg.get("magnet_smooth", 3), 1, 10),
        _s("magnet_fov", "FOV", cfg.get("magnet_fov", 50), 10, 200),
    ])

    # --- Tab: Recoil ---
    tab_recoil = "".join([
        _s("recoil_x", "Recoil X", cfg.get("recoil_x", 0), -10, 10),
        _s("recoil_y", "Recoil Y", cfg.get("recoil_y", 0), -10, 10),
        _s("recoil_duration", "Duration", cfg.get("recoil_duration", 0), 0, 5, 0.1),
        _t("recoil_auto", "Auto", cfg.get("recoil_auto", False)),
    ])

    return f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Bura</title>
<style>
*{{margin:0;padding:0;box-sizing:border-box}}
body{{background:#0d0d0d;color:#bbb;font-family:system-ui,sans-serif;display:flex;justify-content:center;padding:16px 8px}}
.sh{{width:100%;max-width:360px}}
.hd{{text-align:center;padding:10px 0 8px}}
.hd h1{{font-size:16px;color:#fff;letter-spacing:.3px}}
.hd p{{font-size:9px;color:#444;margin-top:2px}}

.tabs{{display:flex;gap:2px;border-bottom:1px solid #1a1a1a;margin-bottom:10px}}
.tabs button{{background:none;border:none;color:#555;font-size:11px;font-weight:600;padding:6px 10px;cursor:pointer;border-bottom:2px solid transparent;transition:.15s}}
.tabs button:hover{{color:#999}}
.tabs button.on{{color:#7c5cfc;border-bottom-color:#7c5cfc}}

.pn{{display:none}}.pn.on{{display:block}}

/* inline slider row */
.s{{display:flex;align-items:center;gap:6px;margin-bottom:8px}}
.s .l{{font-size:10px;color:#777;min-width:58px;flex-shrink:0}}
.s .v{{font-size:10px;color:#7c5cfc;font-weight:600;min-width:30px;text-align:right}}
.s input[type=range]{{flex:1;-webkit-appearance:none;height:3px;border-radius:2px;background:#1a1a1a;outline:none}}
.s input[type=range]::-webkit-slider-thumb{{-webkit-appearance:none;width:12px;height:12px;border-radius:50%;background:#7c5cfc;cursor:pointer}}
.s input[type=range]::-moz-range-thumb{{width:12px;height:12px;border-radius:50%;background:#7c5cfc;cursor:pointer;border:none}}
.s select{{flex:1;padding:4px 6px;background:#141414;color:#ccc;border:1px solid #222;border-radius:4px;font-size:11px;outline:none}}
.s select:focus{{border-color:#7c5cfc}}

/* toggle row group */
.tg{{display:flex;gap:14px;flex-wrap:wrap;margin:6px 0 4px}}
.tg .s{{margin-bottom:0}}

/* toggle switch */
.sw{{position:relative;width:30px;height:16px;flex-shrink:0}}
.sw input{{opacity:0;width:0;height:0}}
.dot{{position:absolute;cursor:pointer;inset:0;background:#222;border-radius:16px;transition:.2s}}
.dot::before{{content:'';position:absolute;width:10px;height:10px;left:3px;top:3px;background:#555;border-radius:50%;transition:.2s}}
.sw input:checked+.dot{{background:#7c5cfc}}
.sw input:checked+.dot::before{{transform:translateX(14px);background:#fff}}

.ab{{text-align:center;padding:12px 0}}
.ab button{{background:#7c5cfc;color:#fff;border:none;padding:7px 36px;border-radius:5px;font-size:12px;font-weight:600;cursor:pointer;transition:.15s}}
.ab button:hover{{background:#6a4ae0}}
</style>
</head>
<body>
<div class="sh">
  <div class="hd"><h1>Bura</h1><p>config</p></div>
  <div class="tabs" id="tabs">
    <button class="on" data-t="aim">Aimbot</button>
    <button data-t="trig">Trigger</button>
    <button data-t="mag">Magnet</button>
    <button data-t="rec">Recoil</button>
  </div>
  <form method="post" action="/">
    <div class="pn on" id="p-aim">{tab_aim}</div>
    <div class="pn" id="p-trig">{tab_trigger}</div>
    <div class="pn" id="p-mag">{tab_magnet}</div>
    <div class="pn" id="p-rec">{tab_recoil}</div>
    <div class="ab"><button type="submit">Apply</button></div>
  </form>
</div>
<script>
document.getElementById('tabs').addEventListener('click',e=>{{
  const b=e.target.closest('button');
  if(!b||!b.dataset.t)return;
  document.querySelectorAll('.tabs button').forEach(x=>x.classList.remove('on'));
  document.querySelectorAll('.pn').forEach(x=>x.classList.remove('on'));
  b.classList.add('on');
  document.getElementById('p-'+b.dataset.t).classList.add('on');
}});
</script>
</body>
</html>"""
