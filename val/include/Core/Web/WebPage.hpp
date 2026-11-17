#pragma once

// split in pieces: MSVC rejects a single string literal over ~16 KB
namespace Core::Web
{
	inline constexpr const char* kCss = R"CSS(
:root{--bg:#0b0b10;--panel:#12121a;--card:#171722;--card2:#1c1c2a;--line:#262636;--line2:#31314a;--tx:#e8e8f2;--tx2:#9a9ab4;--tx3:#5f5f7a;--ac:#7c5cfc;--ac2:#a48bff;--acg:rgba(124,92,252,.28)}
*{margin:0;padding:0;box-sizing:border-box}html{color-scheme:dark}
body{background:radial-gradient(900px 420px at 50% -120px,rgba(124,92,252,.16),transparent 70%),var(--bg);color:var(--tx);font:13px/1.45 Inter,"Segoe UI",system-ui,sans-serif;min-height:100vh;display:flex;justify-content:center;padding:20px 12px}
.wrap{width:100%;max-width:440px}
.hd{display:flex;align-items:center;justify-content:space-between;margin-bottom:14px}
.brand{display:flex;align-items:center;gap:9px}
.logo{width:34px;height:34px;border-radius:10px;display:grid;place-items:center;background:linear-gradient(135deg,var(--ac),#5b3fe0);color:#fff;font-weight:800;font-size:16px;box-shadow:0 6px 18px var(--acg)}
h1{font-size:17px;font-weight:700}small{display:block;font-size:11px;color:var(--tx3)}
.badge{display:flex;align-items:center;gap:7px;font-size:11px;color:var(--tx2);background:var(--panel);border:1px solid var(--line);padding:4px 11px;border-radius:99px}
.badge i{width:7px;height:7px;border-radius:50%;background:#3ddc97;box-shadow:0 0 8px #3ddc97}.badge.off i{background:#e5484d;box-shadow:0 0 8px #e5484d}
.tabs{display:grid;grid-template-columns:repeat(5,1fr);gap:4px;padding:4px;margin-bottom:14px;background:var(--panel);border:1px solid var(--line);border-radius:12px}
.tabs button{background:none;border:none;color:var(--tx3);font:600 12px inherit;font-family:inherit;padding:8px 0;border-radius:8px;cursor:pointer}
.tabs button:hover{color:var(--tx2)}.tabs button.on{color:#fff;background:var(--ac);box-shadow:0 4px 14px var(--acg)}
.pn{display:none;flex-direction:column;gap:8px}.pn.on{display:flex}
.card{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:16px 18px;display:flex;flex-direction:column;gap:16px}
.card>*+*{padding-top:16px;border-top:1px solid var(--line)}
.g2{display:grid;grid-template-columns:1fr 1fr;gap:14px 20px}.g3{display:grid;grid-template-columns:repeat(3,1fr);gap:14px}.g3 .v{min-width:0;padding:0 4px}.g2>*,.g3>*{min-width:0}.tl{display:flex;gap:8px}.tl .t{flex:1;padding:10px 8px;gap:6px}.tl .l{white-space:nowrap}
.ab .btn{width:100%;padding:14px;margin-top:6px;font-size:13px;color:#fff;border:none;border-radius:12px;background:linear-gradient(135deg,var(--ac),#5b3fe0);box-shadow:0 8px 22px var(--acg)}
.s{display:flex;flex-direction:column;gap:6px}.sl{display:flex;justify-content:space-between}
.l{font-size:12px;color:var(--tx2)}
.v{font-size:12px;font-weight:700;color:var(--ac2);background:rgba(124,92,252,.12);padding:1px 8px;border-radius:6px;min-width:34px;text-align:center}
input[type=range]{-webkit-appearance:none;appearance:none;width:100%;height:18px;background:transparent;cursor:pointer}
input[type=range]::-webkit-slider-runnable-track{height:4px;border-radius:4px;background:linear-gradient(90deg,var(--ac) var(--p,50%),var(--line2) var(--p,50%))}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:16px;height:16px;margin-top:-6px;border-radius:50%;background:#fff;border:3px solid var(--ac)}
select,input[type=text]{width:100%;padding:9px 12px;background:var(--card2);color:var(--tx);border:1px solid var(--line2);border-radius:8px;font:inherit;outline:none}
select:focus,input[type=text]:focus{border-color:var(--ac);box-shadow:0 0 0 3px var(--acg)}
.t{display:flex;align-items:center;justify-content:space-between;padding:10px 12px;background:var(--card2);border:1px solid var(--line);border-radius:8px}
.sw{position:relative;width:36px;height:20px}.sw input{position:absolute;opacity:0;width:100%;height:100%;margin:0;cursor:pointer;z-index:1}
.dot{position:absolute;inset:0;background:var(--line2);border-radius:20px;transition:.2s}
.dot::before{content:'';position:absolute;width:14px;height:14px;left:3px;top:3px;background:#8a8aa6;border-radius:50%;transition:.2s}
.sw input:checked+.dot{background:var(--ac)}.sw input:checked+.dot::before{transform:translateX(16px);background:#fff}
.row{display:flex;gap:8px;align-items:center}.row>*{flex:1}
.btn{background:var(--card2);color:var(--tx);border:1px solid var(--line2);border-radius:8px;padding:9px 12px;font:600 12px inherit;font-family:inherit;cursor:pointer}
.btn:hover{background:var(--ac);border-color:var(--ac)}
pre{background:var(--card2);border:1px solid var(--line);border-radius:8px;padding:10px;font:11px/1.5 Consolas,monospace;white-space:pre-wrap;color:var(--tx2)}
img{width:100%;image-rendering:pixelated;border-radius:8px;border:1px solid var(--line);background:#000}
)CSS";

	inline constexpr const char* kBody = R"HTML(<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Bura</title><style>)HTML";

	inline constexpr const char* kBody2 = R"HTML(</style></head><body><div class="wrap">
<header class="hd"><div class="brand"><div class="logo">B</div><div><h1>Bura</h1><small>Configuration</small></div></div><div class="badge" id="bd"><i></i><span id="bt">Running</span></div></header>
<nav class="tabs" id="tabs"><button class="on" data-t="aim">Aimbot</button><button data-t="trig">Trigger</button><button data-t="key">Keys</button><button data-t="inp">Input</button><button data-t="dbg">Debug</button></nav>
<div class="pn on" id="p-aim"></div><div class="pn" id="p-trig"></div><div class="pn" id="p-key"></div><div class="pn" id="p-inp"></div>
<div class="pn" id="p-dbg"><div class="card"><img id="pv" alt="preview"><pre id="di">...</pre></div></div>
<div class="ab"><button class="btn" id="apply">Apply</button></div>
</div><script>)HTML";

	inline constexpr const char* kJs = R"JS(
var $=function(i){return document.getElementById(i)},cfg=null,timers={};
var COLORS=['magenta','yellow','red'],MODES=['hold','toggle','auto'];
function post(u,b){return fetch(u,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(b||{})}).then(function(r){return r.json()})}
function send(sec,patch){var o={};o[sec+'_']=patch;var k=sec+Object.keys(patch)[0];clearTimeout(timers[k]);timers[k]=setTimeout(function(){post('/api/config',o)},120)}
function el(h,c){var d=document.createElement('div');if(c)d.className=c;d.innerHTML=h;return d}
function slider(sec,key,label,min,max,step){var w=el('<div class="sl"><span class="l">'+label+'</span><span class="v"></span></div><input type="range" min="'+min+'" max="'+max+'" step="'+step+'">','s');
 var r=w.querySelector('input'),t=w.querySelector('.v');r.value=cfg[sec+'_'][key];
 function up(){t.textContent=step<1?(+r.value).toFixed(2):r.value;r.style.setProperty('--p',((r.value-min)/(max-min)*100)+'%')}
 up();r.oninput=function(){up();var p={};p[key]=+r.value;send(sec,p)};return w}
function toggle(sec,key,label){var w=el('<span class="l" style="color:var(--tx)">'+label+'</span><label class="sw"><input type="checkbox"><span class="dot"></span></label>','t');
 var c=w.querySelector('input');c.checked=!!cfg[sec+'_'][key];c.onchange=function(){var p={};p[key]=c.checked;send(sec,p)};return w}
function select(label,opts,val,on){var w=el('<span class="l">'+label+'</span><select></select>','s');var s=w.querySelector('select');
 opts.forEach(function(o,i){var e=document.createElement('option');e.value=i;e.textContent=o;s.appendChild(e)});s.value=val;s.onchange=function(){on(+s.value)};return w}
function grp(cls,items){var g=el('',cls);items.forEach(function(i){g.appendChild(i)});return g}
function bindRow(sec,label){var k=cfg[sec+'_'].bind,w=el('<span class="l">'+label+'</span><div class="row"><button class="btn"></button><select><option value="0">Hold</option><option value="1">Toggle</option></select></div>','s');
 var b=w.querySelector('button'),s=w.querySelector('select');b.textContent=cfg._names[sec]||'Unbound';s.value=k.mode==='Toggle'?1:0;
 b.onclick=function(){b.textContent='press a key (Esc = clear)...';post('/api/bind',{name:sec}).then(function(r){b.textContent=r.name||'Unbound'})};
 s.onchange=function(){send(sec,{bind_mode:+s.value})};return w}
function mode(){var a=cfg.aimbot_;return a.auto_aim?2:(a.bind.mode==='Toggle'?1:0)}
function build(){var A=cfg.aimbot_,P=$('p-aim'),c;
 P.innerHTML='';c=el('','card');P.appendChild(c);
 c.appendChild(grp('g2',[select('Color',COLORS,cfg.settings_.color,function(v){send('settings',{color:v})}),
  select('Mode',MODES,mode(),function(v){send('aimbot',{auto_aim:v===2,bind_mode:v===1?1:0})})]));
 c.appendChild(grp('g2',[slider('aimbot','fov_x','FOV X',20,250,1),slider('aimbot','fov_y','FOV Y',20,250,1)]));
 c.appendChild(grp('g2',[slider('aimbot','lock_gain','Gain',0.1,5,0.05),slider('aimbot','smooth','Smooth',0.1,1,0.05)]));
 c.appendChild(grp('g2',[slider('aimbot','hold_pct','Hold %',0,150,1),slider('aimbot','lock_settle_ms','Settle ms',1,30,1)]));
 c.appendChild(grp('g2',[slider('aimbot','speed_x','Speed X',0.2,3,0.05),slider('aimbot','speed_y','Speed Y',0.2,3,0.05)]));
 c.appendChild(grp('g3',[slider('aimbot','reaction_ms','Reaction ms',0,150,1),slider('aimbot','offset_x','X Offset',-15,15,1),slider('aimbot','offset_y','Y Offset',-15,15,1)]));
 c.appendChild(slider('aimbot','tolerance','Tolerance',0,60,1));
 c.appendChild(grp('tl',[toggle('aimbot','anti_strafe','Anti-Strafe'),toggle('aimbot','humanize','Humanize'),toggle('aimbot','legit','Legit')]));
 P=$('p-trig');P.innerHTML='';c=el('','card');P.appendChild(c);
 c.appendChild(grp('g2',[slider('trigger','scan_area','Scan area',1,30,1),slider('trigger','delay','Delay ms',1,300,1)]));
 c.appendChild(grp('tl',[toggle('trigger','move_check','Move check')]));
 P=$('p-key');P.innerHTML='';c=el('','card');P.appendChild(c);
 c.appendChild(bindRow('aimbot','Aimbot bind'));c.appendChild(bindRow('trigger','Trigger bind'));
 P=$('p-inp');P.innerHTML='';c=el('','card');P.appendChild(c);
 c.appendChild(select('Input',['SendInput (local)','Makcu (serial)','Medius (capi dll)'],cfg.settings_.input_backend,function(v){send('settings',{input_backend:v})}));
 var w=el('<span class="l">COM port</span><input type="text">','s'),i=w.querySelector('input');i.value=cfg.settings_.com_port;i.oninput=function(){send('settings',{com_port:i.value})};c.appendChild(w);
 var st=el('','');st.innerHTML='<div class="row"><button class="btn" id="ap">Apply input</button></div><pre id="ms" style="margin-top:10px"></pre>';c.appendChild(st);$('ms').textContent=cfg._mouse||'';
 $('ap').onclick=function(){post('/api/input/apply').then(function(x){$('ms').textContent=x.status||''})}}
document.querySelectorAll('#tabs button').forEach(function(b){b.onclick=function(){document.querySelectorAll('#tabs button,.pn').forEach(function(e){e.classList.remove('on')});b.classList.add('on');$('p-'+b.dataset.t).classList.add('on')}});
$('apply').onclick=function(){var b=$('apply');post('/api/input/apply').then(function(x){b.textContent='Applied';setTimeout(function(){b.textContent='Apply'},900)})};
function load(){return fetch('/api/config').then(function(r){return r.json()}).then(function(j){cfg=j;build()})}
function dbg(){if(!$('p-dbg').classList.contains('on'))return;fetch('/api/debug').then(function(r){return r.json()}).then(function(d){
 $('di').textContent=['captured : '+d.captured,'frame    : '+d.cap_w+'x'+d.cap_h,'lit px   : '+d.lit,'blobs    : '+d.big_blobs+' / '+d.blobs,'found    : '+d.found,'on target: '+d.on_target,'aim/trig : '+d.aim_active+' / '+d.trigger_active,'dist     : '+d.dist.toFixed(1),'mean BGR : '+d.mean_bgr.join(','),'ctr HSV  : '+d.center_hsv.join(','),'frames   : '+d.frames].join('\n');
 $('pv').src='/api/preview.png?'+Date.now()}).catch(function(){})}
function ping(){fetch('/api/ping').then(function(){$('bd').className='badge';$('bt').textContent='Running'}).catch(function(){$('bd').className='badge off';$('bt').textContent='Offline'})}
load();setInterval(dbg,500);setInterval(ping,3000);
)JS";

	inline constexpr const char* kEnd = "</script></body></html>";
}
