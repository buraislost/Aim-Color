"""
WebGui.py — Flask WebGUI HTML template
Tabbed config panel — Bura
"""

from user32 import vk_name

_CSS = """
:root{
  --bg:#0b0b10;--panel:#12121a;--card:#171722;--card2:#1c1c2a;
  --line:#262636;--line2:#31314a;
  --tx:#e8e8f2;--tx2:#9a9ab4;--tx3:#5f5f7a;
  --ac:#7c5cfc;--ac2:#a48bff;--acg:rgba(124,92,252,.28);
  --r:12px;--r2:8px;
}
*{margin:0;padding:0;box-sizing:border-box}
html{color-scheme:dark}
body{
  background:radial-gradient(900px 420px at 50% -120px,rgba(124,92,252,.16),transparent 70%),var(--bg);
  color:var(--tx);font:13px/1.45 Inter,"Segoe UI",system-ui,sans-serif;
  min-height:100vh;display:flex;justify-content:center;align-items:flex-start;padding:20px 12px;
  -webkit-font-smoothing:antialiased
}
.wrap{width:100%;max-width:440px;transition:max-width .15s}.wrap.wide{max-width:1180px}

/* header */
.hd{display:flex;align-items:center;justify-content:space-between;margin-bottom:14px}
.brand{display:flex;align-items:center;gap:9px}
.logo{
  width:34px;height:34px;border-radius:10px;display:grid;place-items:center;
  background:linear-gradient(135deg,var(--ac),#5b3fe0);color:#fff;font-weight:800;font-size:16px;
  box-shadow:0 6px 18px var(--acg)
}
.hd h1{font-size:17px;font-weight:700;letter-spacing:.2px;line-height:1.1}
.hd small{display:block;font-size:11px;color:var(--tx3);margin-top:2px}
.badge{
  display:flex;align-items:center;gap:7px;font-size:11px;color:var(--tx2);
  background:var(--panel);border:1px solid var(--line);padding:4px 11px;border-radius:99px
}
.badge i{width:7px;height:7px;border-radius:50%;background:#3ddc97;box-shadow:0 0 8px #3ddc97}

/* tabs */
.tabs{
  display:grid;grid-template-columns:repeat(7,1fr);gap:4px;padding:4px;margin-bottom:14px;
  background:var(--panel);border:1px solid var(--line);border-radius:var(--r)
}
.tabs button{
  background:none;border:none;color:var(--tx3);font:600 12px inherit;font-family:inherit;
  padding:8px 0;border-radius:var(--r2);cursor:pointer;transition:.15s;white-space:nowrap
}
.tabs button:hover{color:var(--tx2);background:rgba(255,255,255,.03)}
.tabs button.on{color:#fff;background:var(--ac);box-shadow:0 4px 14px var(--acg)}
.tabs button:focus-visible,.btn:focus-visible{outline:2px solid var(--ac2);outline-offset:2px}

/* panels + cards */
.pn{display:none}
.pn.on{display:flex;flex-direction:column;gap:8px;animation:in .18s ease}
@keyframes in{from{opacity:0;transform:translateY(4px)}to{opacity:1;transform:none}}
.card{background:var(--card);border:1px solid var(--line);border-radius:var(--r);padding:16px 18px}
.ct{
  font-size:9.5px;font-weight:700;letter-spacing:.8px;text-transform:uppercase;
  color:var(--tx3);margin-bottom:8px
}
.g2{display:grid;grid-template-columns:1fr 1fr;gap:14px 20px}
.card{display:flex;flex-direction:column;gap:18px}
.card > * + *{padding-top:16px;border-top:1px solid var(--line)}
.card > .tl{padding-top:16px}
.g3{grid-template-columns:repeat(3,1fr);gap:14px}
.g3 .v{min-width:0;padding:0 4px}

/* slider */
.s{display:flex;flex-direction:column;gap:6px;min-width:0}
.sl{display:flex;justify-content:space-between;align-items:baseline}
.l{font-size:12px;color:var(--tx2)}
.v{
  font-size:12px;font-weight:700;color:var(--ac2);font-variant-numeric:tabular-nums;
  background:rgba(124,92,252,.12);padding:1px 8px;border-radius:6px;min-width:34px;text-align:center
}
input[type=range]{
  -webkit-appearance:none;appearance:none;width:100%;height:18px;background:transparent;outline:none;cursor:pointer;
  --p:50%
}
input[type=range]::-webkit-slider-runnable-track{
  height:4px;border-radius:4px;
  background:linear-gradient(90deg,var(--ac) var(--p),var(--line2) var(--p))
}
input[type=range]::-moz-range-track{height:4px;border-radius:4px;background:var(--line2)}
input[type=range]::-moz-range-progress{height:4px;border-radius:4px;background:var(--ac)}
input[type=range]::-webkit-slider-thumb{
  -webkit-appearance:none;width:16px;height:16px;margin-top:-6px;border-radius:50%;
  background:#fff;border:3px solid var(--ac);box-shadow:0 2px 8px rgba(0,0,0,.5);transition:.12s
}
input[type=range]::-moz-range-thumb{
  width:10px;height:10px;border-radius:50%;background:#fff;border:3px solid var(--ac)
}
input[type=range]:hover::-webkit-slider-thumb{box-shadow:0 0 0 5px var(--acg)}
input[type=range]:focus-visible::-webkit-slider-thumb{box-shadow:0 0 0 5px var(--acg)}

/* select */
select{
  width:100%;padding:9px 30px 9px 12px;background:var(--card2);color:var(--tx);
  border:1px solid var(--line2);border-radius:var(--r2);font:inherit;outline:none;cursor:pointer;
  -webkit-appearance:none;appearance:none;transition:.15s;
  background-image:url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='10' height='6'%3E%3Cpath d='M1 1l4 4 4-4' fill='none' stroke='%239a9ab4' stroke-width='1.6' stroke-linecap='round'/%3E%3C/svg%3E");
  background-repeat:no-repeat;background-position:right 12px center
}
select:hover{border-color:var(--tx3)}
select:focus{border-color:var(--ac);box-shadow:0 0 0 3px var(--acg)}
select option{background:var(--card2)}

/* toggle rows */
.tl{display:flex;gap:10px}
.t{flex:1;display:flex;align-items:center;justify-content:space-between;gap:8px;padding:10px 12px;background:var(--card2);border:1px solid var(--line);border-radius:var(--r2)}
.t .l{color:var(--tx);font-size:12px;white-space:nowrap}
.sw{position:relative;width:36px;height:20px;flex-shrink:0;display:block}
.sw input{position:absolute;opacity:0;width:100%;height:100%;margin:0;cursor:pointer;z-index:1}
.dot{position:absolute;inset:0;background:var(--line2);border-radius:20px;transition:.2s}
.dot::before{
  content:'';position:absolute;width:14px;height:14px;left:3px;top:3px;
  background:#8a8aa6;border-radius:50%;transition:.2s
}
.sw input:checked + .dot{background:var(--ac)}
.sw input:checked + .dot::before{transform:translateX(16px);background:#fff}
.sw input:focus-visible + .dot{box-shadow:0 0 0 3px var(--acg)}

/* input source picker */
.opts{display:grid;grid-template-columns:1fr 1fr;gap:12px}
.opt{position:relative;display:block;cursor:pointer}
.opt input{position:absolute;opacity:0;inset:0;margin:0;cursor:pointer;z-index:1}
.ob{
  height:100%;display:flex;flex-direction:column;gap:5px;padding:14px;
  background:var(--card2);border:1.5px solid var(--line2);border-radius:var(--r);transition:.15s
}
.opt:hover .ob{border-color:var(--tx3)}
.opt input:checked + .ob{border-color:var(--ac);background:rgba(124,92,252,.1);box-shadow:0 0 0 3px var(--acg)}
.opt input:focus-visible + .ob{outline:2px solid var(--ac2);outline-offset:2px}
.ob b{font-size:14px;color:var(--tx)}
.ob small{font-size:11px;color:var(--tx3);line-height:1.4}
.st{
  margin-top:auto;padding-top:6px;font-size:11px;font-weight:600;display:flex;align-items:center;gap:6px;color:var(--tx3)
}
.st::before{content:'';width:7px;height:7px;border-radius:50%;background:var(--tx3)}
.st.ok{color:#3ddc97}.st.ok::before{background:#3ddc97;box-shadow:0 0 8px #3ddc97}
.st.no{color:#ff7a7a}.st.no::before{background:#ff7a7a}
.btn2{
  width:100%;padding:10px;border-radius:var(--r2);cursor:pointer;font:600 12px inherit;font-family:inherit;
  background:transparent;color:var(--ac2);border:1px solid var(--ac);transition:.15s
}
.btn2:hover{background:rgba(124,92,252,.14)}
.hint{font-size:11px;color:var(--tx3);line-height:1.5}

/* debug tab */
.card.dbg > * + *{padding-top:0;border-top:none}
.shot{background:#000;border-radius:var(--r2);min-height:130px;display:flex;align-items:center;justify-content:center;overflow:hidden}
.shot img{width:100%;max-height:260px;object-fit:contain;image-rendering:pixelated;display:block}
.shot img:not([src]){display:none}
.dtop{display:grid;grid-template-columns:1fr 1fr;gap:12px;align-items:stretch}
.dtop>div{display:flex;flex-direction:column;gap:6px;min-width:0}
.dtop .shot{min-height:0}
.dtop .shot img{width:100%;height:100%;max-height:none;object-fit:contain}
.dtop .big{aspect-ratio:16/9}
.dside .shot{flex:1;min-height:120px}
.dside .cap{margin-top:0}
.cap{font-size:11px;color:var(--tx3);margin-top:6px;line-height:1.4}
.lg{display:inline-block;width:8px;height:8px;border-radius:2px;margin:0 4px 0 8px;vertical-align:baseline}
.lg:first-child{margin-left:0}.lg.g{background:#3cff3c}.lg.b{background:#3ca0ff}.lg.w{background:#fff}.lg.r{background:#ff4040}
.chips{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}
.chip{background:var(--card2);border:1px solid var(--line);border-radius:var(--r2);padding:8px 10px;display:flex;flex-direction:column;gap:2px;min-width:0}
.chip small{font-size:10px;color:var(--tx3);text-transform:uppercase;letter-spacing:.05em}
.chip b{font-size:14px;font-weight:600;color:var(--tx);font-variant-numeric:tabular-nums;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.grps{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;align-items:start}
details.grp{background:var(--card);border:1px solid var(--line);border-radius:var(--r);padding:0 14px}
details.grp summary{cursor:pointer;list-style:none;padding:12px 0;font-size:12px;font-weight:600;color:var(--tx2);display:flex;justify-content:space-between}
details.grp summary::-webkit-details-marker{display:none}
details.grp summary::after{content:'+';color:var(--tx3)}
details.grp[open] summary{color:var(--tx)}
details.grp[open] summary::after{content:'−'}
details.grp .gb{display:flex;flex-direction:column;gap:10px;padding-bottom:14px}
@media(max-width:820px){.grps{grid-template-columns:1fr}.chips{grid-template-columns:repeat(2,1fr)}}
@media(max-width:820px){.dtop{grid-template-columns:1fr}}
.kv{display:grid;grid-template-columns:auto 1fr;gap:7px 16px;font-size:12px}
.kv span:nth-child(odd){color:var(--tx3)}
.kv span:nth-child(even){color:var(--tx);text-align:right;font-variant-numeric:tabular-nums;overflow-wrap:anywhere;min-width:0}
.diag{padding:10px 12px;border-radius:var(--r2);font-size:12px;font-weight:600;background:var(--card2);border:1px solid var(--line2);color:var(--tx2)}
.diag.ok{color:#3ddc97;border-color:rgba(61,220,151,.45)}
.diag.bad{color:#ff9a7a;border-color:rgba(255,122,122,.45)}
.row{display:flex;gap:8px}.row .btn2{flex:1}
pre.out{
  font:11px/1.5 ui-monospace,Consolas,monospace;color:var(--tx2);white-space:pre-wrap;word-break:break-all;
  background:var(--card2);border:1px solid var(--line);border-radius:var(--r2);padding:10px;max-height:220px;overflow:auto
}
.pr{display:flex;justify-content:space-between;align-items:center;gap:8px;padding:3px 0}
.pr button{
  background:var(--ac);color:#fff;border:none;border-radius:6px;padding:3px 12px;font:600 11px inherit;font-family:inherit;cursor:pointer
}

/* apply bar */
.ab{
  margin-top:14px
}
.btn{
  width:100%;padding:12px;border:none;border-radius:var(--r);cursor:pointer;
  background:linear-gradient(135deg,var(--ac),#5b3fe0);color:#fff;font:700 13px inherit;font-family:inherit;
  letter-spacing:.3px;box-shadow:0 8px 22px var(--acg);transition:.15s
}
.btn:hover{filter:brightness(1.1);transform:translateY(-1px)}
.btn:active{transform:none}

@media (max-width:380px){.g2{grid-template-columns:1fr}}
"""

_JS = r"""
(function(){
  var tabs=document.getElementById('tabs'),KEY='bura_tab';
  // MJPEG streams: swapping src to a 1px gif is what actually aborts the request in the browser
  var NIL='data:image/gif;base64,R0lGODlhAQABAAAAACw=';
  function streamsOn(){
    var sc=document.getElementById('d_scr'),ro=document.getElementById('d_img');
    if(!sc||!ro)return;window._strm=1;window._dead=0;
    sc.src='/api/debug/stream/screen?'+Date.now();ro.src='/api/debug/stream/roi?'+Date.now();
  }
  function streamsOff(){
    var sc=document.getElementById('d_scr'),ro=document.getElementById('d_img');
    window._strm=0;if(sc)sc.src=NIL;if(ro)ro.src=NIL;
  }
  document.addEventListener('visibilitychange',function(){
    if(document.hidden){if(window._strm)streamsOff()}
    else if(window._tab==='dbg'&&!window._strm)streamsOn();
  });
  window.addEventListener('pagehide',function(){if(window._strm)streamsOff()});
  function show(t){
    var b=tabs.querySelector('[data-t="'+t+'"]'),p=document.getElementById('p-'+t);
    if(!b||!p)return false;
    document.querySelectorAll('.tabs button').forEach(function(x){x.classList.remove('on')});
    document.querySelectorAll('.pn').forEach(function(x){x.classList.remove('on')});
    b.classList.add('on');p.classList.add('on');
    document.querySelector('.wrap').classList.toggle('wide',t==='dbg');
    window._tab=t;
    if(t==='dbg'&&!document.hidden){if(!window._strm)streamsOn()}else if(window._strm)streamsOff();
    try{localStorage.setItem(KEY,t)}catch(e){}
    return true;
  }
  tabs.addEventListener('click',function(e){
    var b=e.target.closest('button');
    if(b&&b.dataset.t)show(b.dataset.t);
  });
  try{var s=localStorage.getItem(KEY);if(s)show(s)}catch(e){}

  // ---- debug tab: live polling only while the tab is open ----
  var panel=document.getElementById('p-dbg'),busy=false;
  function $(i){return document.getElementById(i)||{}}
  function set(id,v){$(id).textContent=(v===null||v===undefined||v==='')?'—':v}
  function lbl(t){return String(t).replace(/\r/g,'\\r').replace(/\n/g,'\\n')}
  function out(t){$('d_out').textContent=t}
  function post(u,b){
    return fetch(u,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(b||{})}).then(function(r){return r.json()});
  }
  function diag(d){
    var t=d.detect||{},l=d.loop||{},i=d.input||{},tr=d.trace,mix=(tr&&tr.reasons)||{},m,c='ok';
    var cp0=d.capture||{};
    var w=d.wide;if(w&&(w.err||!w.t||(Date.now()/1000-w.t)>3))w=null;
    var mv=mix.move||0,np=mix.no_pixels||0,dz=(mix['deadzone']||0)+(mix['deadzone(locked)']||0),sp=mix.subpixel||0;
    if(l.err){m='Loop error: '+l.err;c='bad'}
    else if(t.err){m='Capture error: '+t.err;c='bad'}
    else if(t.mean!=null&&t.mean<1){m='Capture is black ('+((d.capture&&d.capture.backend)||'?')+'): the game is not being grabbed. Try the other backend (DXGI / GDI buttons) or check display mode / monitor.';c='bad'}
    else if(cp0.src==='screen'&&cp0.reason){m='Game window is not tracked: '+cp0.reason+'. The capture box sits on the screen center, not on the crosshair.';c='bad'}
    else if(!tr||!tr.calls){m='Aim is not sampling: mode "'+d.aim_mode+'" needs its key (see Keys tab), or capture has not started.';c='bad'}
    else if(mv>0&&i.mode==='makcu'&&!i.makcu_found){m='Makcu not found: moves are going through SendInput fallback.';c='ok'}
    else if(mv>0&&i.mode!=='makcu'){m='Input mode is SendInput: Vanguard ignores it inside Valorant. Switch to Makcu.';c='bad'}
    else if(mv>=15&&tr.stuck){m='Aim sends moves but the offset to the target is not shrinking ('+tr.off0+' px -> '+tr.off1+' px): the game is not receiving them, or the capture center is not on the crosshair (see Capture center).';c='bad'}
    else if(mv>=8&&tr.flips>=6){m='Aim overshoots: the offset flips sign '+tr.flips+' times in 5 s. Raise Speed X / Y (bigger = slower).';c='bad'}
    else if(mv>0){m='OK: aim sent '+tr.sent[0]+' / '+tr.sent[1]+' px in 5 s ('+mv+' move samples).';c='ok'}
    else if(dz>=np&&dz>=sp&&dz>0){m='Target is inside the dead zone: lower Dead X / Dead Y.';c='bad'}
    else if(sp>np){m='Moves round down to 0: Speed is too high (smaller = faster).';c='bad'}
    else if(w&&w['in']===0&&w.out>0&&w.need){m='Color is in view ('+w.dx+', '+w.dy+' px from the crosshair) but outside the FOV box: raise FOV X / Y to '+w.need+' or more.';c='bad'}
    else if(w&&w['in']===0&&w.out===0){m='No pixel of "'+d.color+'" in the whole wide view: no bot on screen, or the game highlight color differs from "'+d.color+'" (set Enemy Highlight to match).';c='bad'}
    else{m='Color is inside the FOV box but no blob was picked (too small, min_blob). See "last frame with color" below.';c='bad'}
    $('d_diag').textContent=m;$('d_diag').className='diag '+c;
  }
  function poll(){
    if(busy)return;busy=true;
    fetch('/api/debug').then(function(r){return r.json()}).then(function(d){
      var t=d.detect||{},l=d.loop||{},i=d.input||{};
      set('d_color',d.color);set('d_px',t.px);
      set('d_tgt',t.target?t.target.join(', '):null);
      set('d_off',t.dx!=null?t.dx+', '+t.dy:null);
      set('d_frame',t.frame?t.frame.join('x')+' (screen '+d.screen.join('x')+')':null);
      var cp=d.capture||{};
      set('d_cap',cp.center?cp.center.join(', ')+' ('+cp.src+(cp.src==='screen'?': '+(cp.reason||('window "'+cp.win+'" not found')):(cp.found&&cp.found.title?': "'+cp.found.title.trim()+'" '+(cp.found.size||[]).join('x'):''))+') ['+(cp.backend||'?')+(cp.err_streak?' err='+cp.err_streak:'')+']':null);
      set('d_mean',t.mean);
      var wd=d.wide,fresh=wd&&!wd.err&&wd.t&&(Date.now()/1000-wd.t)<3;
      set('d_wide',fresh?(wd['in']+' px in FOV, '+wd.out+' px outside'+(wd.need?' (nearest '+wd.dx+', '+wd.dy+')':'')):(wd&&wd.err)||null);
      set('d_active',l.active?'ON':'OFF');
      set('d_key',(l.hotkey_down?'down':'up')+' / '+d.aim_mode);
      set('d_hz',l.hz?l.hz+' Hz':null);
      set('d_moves',t.moves);
      set('d_last',t.last_move?t.last_move.join(', '):null);
      set('d_in',i.mode+(i.mode==='makcu'?(i.makcu_found?' ('+i.makcu_port+' @ '+i.baud+')':' (not found)'):''));
      var tr=d.trace;
      set('d_fg',d.fg);
      set('d_rate',tr?tr.rate+' /s (active '+Math.round((l.active_ratio||0)*100)+'%)':null);
      set('d_mix',tr&&tr.calls?Object.keys(tr.reasons).map(function(k){return k+' '+tr.reasons[k]}).join(', '):null);
      set('d_lastS',tr&&tr.last?tr.last.why+' px='+tr.last.px+' d=('+tr.last.dx+', '+tr.last.dy+') mv=('+tr.last.mx+', '+tr.last.my+')':null);
      set('d_sent',tr?tr.sent.join(', '):null);
      if(tr&&tr.hit_t&&tr.hit_t!==window._hitT){
        window._hitT=tr.hit_t;
        var hi=new Image();hi.onload=function(){$('d_hit').src=hi.src};hi.src='/api/debug/hit.png?'+tr.hit_t;
      }
      if(tr&&tr.hit_age!=null)$('d_hitage').textContent='Last frame where the color was seen: '+tr.hit_age+' s ago.';
      diag(d);
      // preload, then swap: assigning src directly blanks the image on every poll
      var sf=d.stream||{};
      set('d_fps',(sf.screen||0)+' / '+(sf.roi||0));
      // watchdog: stream reports 0 fps while Debug is open -> reopen it
      if(window._strm&&!(sf.screen>0)){if(++window._dead>=4){streamsOff();streamsOn()}}else window._dead=0;
    }).catch(function(){}).then(function(){busy=false});
  }
  setInterval(function(){if(panel.classList.contains('on')&&!document.hidden)poll()},500);

  $('b_info').onclick=function(){
    out('Querying...');
    post('/api/makcu/info').then(function(r){
      var s='Port: '+(r.port||'none')+'\nBaud: '+r.baud+'   Terminator: '+lbl(r.term)+'\nOpen: '+r.open+
            '\nkm.version() -> '+JSON.stringify(r.version||'')+'\n\nSerial ports (* = known Makcu VID:PID):\n';
      r.ports.forEach(function(p){s+=(p.match?'* ':'  ')+p.device+'  '+p.hwid+'  '+p.desc+'\n'});
      if(!r.ports.length)s+='  (none)\n';
      out(s);
    }).catch(function(e){out('Request failed: '+e)});
  };
  $('b_test').onclick=function(){
    post('/api/makcu/test').then(function(r){
      var lines = [];
      lines.push(r.ok ? 'MAKCU OK — cursor moved.' : 'MAKCU PROBLEM — details below.');
      lines.push('Port: '+(r.port||'none')+'  Baud: '+r.baud+'  Term: '+r.term+'  Open: '+r.open);
      lines.push('Ports now: '+(r.ports_now||[]).join(', '));
      lines.push('Version replies (5x): '+(r.versions||[]).map(function(v,i){return '['+i+']'+v}).join(' | '));
      lines.push('Moves written: '+r.write_count+'/4');
      if(r.move_errors && r.move_errors.length) lines.push('Move errors: '+r.move_errors.join('; '));
      if(!r.ok){
        var empties = (r.versions||[]).filter(function(v){return v==='(empty)'||v.indexOf('ERR')===0}).length;
        if(empties>=3) lines.push('>> Most versions empty/err: baud rate mismatch. Press Probe baud.');
        else if(empties>0) lines.push('>> Intermittent replies: unstable USB/cable/power. Try different USB port (prefer 2.0 on motherboard, not hub).');
        if(!r.open) lines.push('>> Port not open: another app holds it, or device disconnected.');
      }
      out(lines.join('\\n'));
    });
  };
  $('b_soft').onclick=$('b_test_soft').onclick=function(){
    post('/api/soft/test').then(function(r){
      out(r.ok?'Sent a 25px square through SendInput. The cursor should have moved.':'SendInput failed.');
    }).catch(function(e){out('Request failed: '+e)});
  };
  function _swCap(bk){
    post('/api/capture/backend',{backend:bk}).then(function(r){
      out(r.ok?('Capture backend switched to '+r.backend+'. If aim was running, it will reinit on the next frame.'):'Switch failed: '+r.err);
    }).catch(function(e){out('Request failed: '+e)});
  }
  if($('b_cap_dxgi').onclick!==undefined){$('b_cap_dxgi').onclick=function(){_swCap('dxgi')};}
  if($('b_cap_gdi').onclick!==undefined){$('b_cap_gdi').onclick=function(){_swCap('gdi')};}
  function _loadWins(){
    post('/api/capture/windows').then(function(r){
      var sel=$('d_win');sel.innerHTML='';
      (r.windows||[]).forEach(function(w){
        var o=document.createElement('option');o.value=(w.title||'').trim();
        o.textContent=(w.title.trim()||'(no title)')+'  ['+w.cls+']  '+w.w+'x'+w.h+' @'+w.x+','+w.y;
        if(o.value&&o.value===r.current)o.selected=true;
        sel.appendChild(o);
      });
      out(r.ok?((r.windows||[]).length+' windows (visible total '+r.total+(r.filtered?'':', filter hid all -> raw list')+'). Pick the game window and press Use (Auto = VALORANT).'):('List failed: '+r.err));
    }).catch(function(e){out('Request failed: '+e)});
  }
  function _useWin(t){
    post('/api/capture/window',{title:t}).then(function(r){out('Capture window = "'+t+'". The Capture center row updates in a second.')})
      .catch(function(e){out('Request failed: '+e)});
  }
  $('b_win_list').onclick=_loadWins;
  $('b_win_use').onclick=function(){var v=$('d_win').value;if(v)_useWin(v);else out('Press Windows first, then pick the game window.')};
  $('b_win_auto').onclick=function(){_useWin('VALORANT')};
  document.querySelectorAll('.kb').forEach(function(b){
    b.onclick=function(){
      var old=b.textContent;b.textContent='Press a key... (Esc cancels)';b.disabled=true;
      post('/api/bind',{name:b.dataset.bind}).then(function(r){b.textContent=r.ok?r.name:old})
        .catch(function(){b.textContent=old}).then(function(){b.disabled=false});
    };
  });
  $('b_probe').onclick=function(){
    out('Probing baud rates, about 5 s...');
    post('/api/makcu/probe').then(function(r){
      var box=$('d_out');box.textContent='';
      if(!r.port){box.textContent='No port with a known Makcu VID:PID. Press Query to list ports.';return}
      box.appendChild(document.createTextNode('Port '+r.port+'. Pick a row that got a reply:'));
      r.results.forEach(function(x){
        var row=document.createElement('div');row.className='pr';
        var tx=document.createElement('span');
        var ok=x.reply&&x.reply.indexOf('ERR')!==0&&x.reply.trim()!=='';
        tx.textContent=x.baud+' '+lbl(x.term)+'  ->  '+(x.reply?JSON.stringify(x.reply).slice(0,44):'(no reply)');
        row.appendChild(tx);
        if(ok){
          var b=document.createElement('button');b.type='button';b.textContent='Use';
          b.onclick=function(){
            post('/api/makcu/use',{baud:x.baud,term:x.term}).then(function(u){
              out('Switched to '+u.baud+' '+lbl(x.term)+'. Connected: '+u.ok+'. Saved to config.');
            });
          };
          row.appendChild(b);
        }
        box.appendChild(row);
      });
    });
  };

  function fill(r){
    var mn=+r.min,mx=+r.max,p=mx>mn?(r.value-mn)/(mx-mn)*100:0;
    r.style.setProperty('--p',p+'%');
  }
  document.querySelectorAll('input[type=range]').forEach(function(r){
    fill(r);
    r.addEventListener('input',function(){
      document.getElementById('v_'+r.name).textContent=r.value;
      fill(r);
    });
  });
})();
"""


def _show_login_form() -> str:
    return """<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Bura</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{min-height:100vh;display:grid;place-items:center;padding:16px;color:#e8e8f2;
  font:13px/1.45 Inter,"Segoe UI",system-ui,sans-serif;
  background:radial-gradient(900px 420px at 50% -120px,rgba(124,92,252,.18),transparent 70%),#0b0b10}
.box{width:100%;max-width:340px;background:#171722;border:1px solid #262636;border-radius:14px;padding:30px 26px;text-align:center}
.logo{width:44px;height:44px;margin:0 auto 14px;border-radius:12px;display:grid;place-items:center;
  background:linear-gradient(135deg,#7c5cfc,#5b3fe0);font-weight:800;font-size:20px;color:#fff;box-shadow:0 6px 18px rgba(124,92,252,.28)}
h2{font-size:18px;margin-bottom:4px}
p{color:#5f5f7a;font-size:11px;margin-bottom:22px}
input{width:100%;padding:11px 14px;background:#1c1c2a;color:#e8e8f2;border:1px solid #31314a;border-radius:8px;
  font:inherit;outline:none;margin-bottom:12px;transition:.15s}
input:focus{border-color:#7c5cfc;box-shadow:0 0 0 3px rgba(124,92,252,.28)}
button{width:100%;padding:11px;border:none;border-radius:8px;cursor:pointer;color:#fff;font:700 13px inherit;
  font-family:inherit;background:linear-gradient(135deg,#7c5cfc,#5b3fe0);transition:.15s}
button:hover{filter:brightness(1.1)}
</style></head><body>
<form class="box" method="post" action="/login">
  <div class="logo">B</div>
  <h2>Bura</h2>
  <p>Enter your license key</p>
  <input name="key" placeholder="License Key" autocomplete="off" autofocus>
  <button type="submit">Login</button>
</form>
</body></html>"""


def get_html(cfg: dict, colors: list, aim_modes: list, magnet_option: list,
             input_status: dict = None) -> str:
    """Generate the main config page HTML with card-based tabbed layout."""

    def _s(name, label, val, min_v=0, max_v=100, step=1):
        """Slider: label ........ value / track"""
        return (
            f'<div class="s"><div class="sl"><span class="l">{label}</span>'
            f'<span class="v" id="v_{name}">{val}</span></div>'
            f'<input type="range" name="{name}" min="{min_v}" max="{max_v}" step="{step}" value="{val}"></div>'
        )

    def _sel(name, label, options, selected):
        opts = "".join(
            f'<option value="{o}" {"selected" if o == selected else ""}>{o}</option>'
            for o in options
        )
        return (
            f'<div class="s"><span class="l">{label}</span>'
            f'<select name="{name}">{opts}</select></div>'
        )

    def _t(name, label, checked):
        chk = "checked" if checked else ""
        return (
            f'<div class="t"><span class="l">{label}</span>'
            f'<label class="sw"><input type="checkbox" name="{name}" {chk}><span class="dot"></span></label></div>'
        )

    def _card(*rows):
        return f'<section class="card">{"".join(rows)}</section>'

    def _g2(a, b):
        return f'<div class="g2">{a}{b}</div>'

    def _g3(a, b, c):
        return f'<div class="g2 g3">{a}{b}{c}</div>'

    def _chips(*items):
        return '<div class="tl">' + "".join(items) + '</div>'

    # --- Tab: Aimbot (single card, fits without scrolling) ---
    tab_aim = _card(
        _g2(
            _sel("color_to_track", "Color", colors, cfg.get("color_to_track", "yellow")),
            _sel("aim_mode", "Mode", aim_modes, cfg.get("aim_mode", "hold")),
        ),
        _g2(
            _s("fovx", "FOV X", cfg.get("fovx", 50), 10, 500),
            _s("fovy", "FOV Y", cfg.get("fovy", 50), 10, 500),
        ),
        _g2(
            _s("speedX", "Speed X", cfg.get("speedX", 3), 1, 30),
            _s("speedY", "Speed Y", cfg.get("speedY", 3), 1, 30),
        ),
        _g2(
            _s("deadzone_x", "Dead X", cfg.get("deadzone_x", 3), 0, 20),
            _s("deadzone_y", "Dead Y", cfg.get("deadzone_y", 3), 0, 20),
        ),
        _g3(
            _s("offset", "Y Offset", cfg.get("offset", 0), -50, 50),
            _s("tolerance", "Tolerance", cfg.get("tolerance", 0), 0, 30),
            _s("game_sens", "Sens", cfg.get("game_sens", 1.0), 0.1, 5.0, 0.1),
        ),
        _chips(
            _t("anti_strafe", "Anti-Strafe", cfg.get("anti_strafe", False)),
            _t("humanize", "Humanize", cfg.get("humanize", False)),
            _t("legitmode", "Legit", cfg.get("legitmode", False)),
        ),
    )

    # --- Tab: Triggerbot ---
    tab_trigger = _card(
        _g2(
            _s("pixel_distance", "Distance", cfg.get("pixel_distance", 5), 1, 30),
            _s("base_delay", "Delay (s)", cfg.get("base_delay", 0.05), 0, 0.5, 0.01),
        ),
        _chips(_t("magnet_triggerbot", "Enable", cfg.get("magnet_triggerbot", False))),
    )

    # --- Tab: Magnet ---
    tab_magnet = _card(
        _sel("magnet_version", "Mode", magnet_option, cfg.get("magnet_version", "v1")),
        _g2(
            _s("magnet_strength", "Strength", cfg.get("magnet_strength", 0.5), 0.1, 2.0, 0.1),
            _s("magnet_smooth", "Smooth", cfg.get("magnet_smooth", 3), 1, 10),
        ),
        _s("magnet_fov", "FOV", cfg.get("magnet_fov", 50), 10, 200),
    )

    # --- Tab: Input (direct mouse vs Makcu) ---
    st = input_status or {}
    mode = cfg.get("input_mode", "makcu")
    if st.get("makcu_found"):
        mk_state = f'<span class="st ok">Connected · {st.get("makcu_port", "")}</span>'
    else:
        mk_state = '<span class="st no">Not found</span>'

    tab_input = _card(
        '<div class="opts">'
        f'<label class="opt"><input type="radio" name="input_mode" value="software" {"checked" if mode == "software" else ""}>'
        '<div class="ob"><b>Input</b><small>Direct mouse input from Windows. No device needed.</small>'
        '<span class="st ok">Always available</span></div></label>'
        f'<label class="opt"><input type="radio" name="input_mode" value="makcu" {"checked" if mode == "makcu" else ""}>'
        f'<div class="ob"><b>Makcu</b><small>Hardware mouse over USB serial.</small>{mk_state}</div></label>'
        '</div>',
        '<button type="submit" class="btn2" formaction="/makcu/connect" formmethod="post">Connect Makcu</button>'
        '<p class="hint">Pick a source, then press Apply. Connect re-scans the COM ports.</p>',
    )

    # --- Tab: Keys (rebindable: any keyboard key or mouse button) ---
    def _key(name, label, vk):
        return (
            f'<div class="s"><span class="l">{label}</span>'
            f'<button type="button" class="btn2 kb" data-bind="{name}">{vk_name(vk)}</button></div>'
        )

    tab_keys = _card(
        _key("hold_key", "Hold key", cfg.get("hold_key", 0x14)),
        _key("toggle_key", "Toggle key", cfg.get("toggle_key", 0x14)),
        _key("magnet_key", "Magnet key", cfg.get("magnet_key", 0x02)),
        '<p class="hint">Hold key is used when Mode = hold, Toggle key when Mode = toggle. '
        'Click, then press any key or mouse button (Esc cancels). Saved instantly.</p>',
    )

    # --- Tab: Debug ---
    def _kv(*pairs):
        return '<div class="kv">' + "".join(
            f'<span>{l}</span><span id="{i}">—</span>' for l, i in pairs) + '</div>'

    def _chip(label, i):
        return f'<div class="chip"><small>{label}</small><b id="{i}">—</b></div>'

    def _grp(title, body, open_=False):
        return (f'<details class="grp"{" open" if open_ else ""}>'
                f'<summary>{title}</summary><div class="gb">{body}</div></details>')

    tab_debug = (
        '<section class="card dbg">'
        '<div class="diag" id="d_diag">Waiting for data...</div>'
        '<div class="dtop">'
        '<div><div class="shot big"><img id="d_scr" alt="full screen"></div>'
        '<div class="cap">Screen &middot; <i class="lg g"></i>aim FOV <i class="lg b"></i>magnet FOV '
        '<i class="lg w"></i>capture center <i class="lg r"></i>real screen center</div></div>'
        '<div class="dside">'
        '<div class="shot"><img id="d_img" alt="FOV box | color mask"></div>'
        '<div class="cap">FOV box | color mask</div>'
        '<div class="chips">'
        + _chip("Pixels", "d_px") + _chip("Target", "d_tgt") + _chip("Offset", "d_off")
        + _chip("Aim", "d_active") + _chip("Loop", "d_hz") + _chip("FPS scr / roi", "d_fps") +
        '</div></div>'
        '</div></section>'
        '<div class="grps">'
        + _grp("Detection", _kv(("Color", "d_color"),
                                ("Frame", "d_frame"), ("Capture center", "d_cap"),
                                ("Brightness", "d_mean"), ("Color scan", "d_wide"), ("Foreground", "d_fg")), True)
        + _grp("Aim loop", _kv(("Key / mode", "d_key"), ("Input", "d_in"), ("Moves sent", "d_moves"),
                               ("Last move", "d_last"), ("Samples / s", "d_rate"),
                               ("Result mix (5 s)", "d_mix"), ("Last sample", "d_lastS"),
                               ("Sent |x|, |y| px", "d_sent")))
        + _grp("Last hit",
               '<div class="shot"><img id="d_hit" alt="last frame with color"></div>'
               '<div class="hint" id="d_hitage">Last frame where the color was seen.</div>')
        + '</div>'
        + _grp("Input tools",
               '<div class="row">'
               '<button type="button" class="btn2" id="b_info">Query</button>'
               '<button type="button" class="btn2" id="b_probe">Probe baud</button>'
               '<button type="button" class="btn2" id="b_test">Test Makcu</button>'
               '<button type="button" class="btn2" id="b_soft">Test SendInput</button>'
               '<button type="button" class="btn2" id="b_cap_dxgi">DXGI</button>'
               '<button type="button" class="btn2" id="b_cap_gdi">GDI</button>'
               '</div>'
               '<div class="row">'
               '<select id="d_win" style="flex:1;min-width:0"><option value="">Game window: press Windows</option></select>'
               '<button type="button" class="btn2" id="b_win_list">Windows</button>'
               '<button type="button" class="btn2" id="b_win_use">Use</button>'
               '<button type="button" class="btn2" id="b_win_auto">Auto</button>'
               '</div>'
               '<pre class="out" id="d_out">Output shows here.</pre>')
    )

    # --- Tab: Recoil ---
    tab_recoil = _card(
        _g3(
            _s("recoil_x", "X", cfg.get("recoil_x", 0), -10, 10),
            _s("recoil_y", "Y", cfg.get("recoil_y", 0), -10, 10),
            _s("recoil_duration", "Duration", cfg.get("recoil_duration", 0), 0, 5, 0.1),
        ),
        _chips(_t("recoil_auto", "Auto", cfg.get("recoil_auto", False))),
    )

    return f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Bura</title>
<style>{_CSS}</style>
</head>
<body>
<div class="wrap">
  <header class="hd">
    <div class="brand">
      <div class="logo">B</div>
      <div><h1>Bura</h1><small>Configuration</small></div>
    </div>
    <div class="badge"><i></i>Running</div>
  </header>
  <nav class="tabs" id="tabs">
    <button type="button" class="on" data-t="aim">Aimbot</button>
    <button type="button" data-t="trig">Trigger</button>
    <button type="button" data-t="mag">Magnet</button>
    <button type="button" data-t="rec">Recoil</button>
    <button type="button" data-t="key">Keys</button>
    <button type="button" data-t="inp">Input</button>
    <button type="button" data-t="dbg">Debug</button>
  </nav>
  <form method="post" action="/">
    <div class="pn on" id="p-aim">{tab_aim}</div>
    <div class="pn" id="p-trig">{tab_trigger}</div>
    <div class="pn" id="p-mag">{tab_magnet}</div>
    <div class="pn" id="p-rec">{tab_recoil}</div>
    <div class="pn" id="p-key">{tab_keys}</div>
    <div class="pn" id="p-inp">{tab_input}</div>
    <div class="pn" id="p-dbg">{tab_debug}</div>
    <div class="ab"><button class="btn" type="submit">Apply</button></div>
  </form>
</div>
<script>{_JS}</script>
</body>
</html>"""
