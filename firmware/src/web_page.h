#pragma once
// Single-file portal UI, served from flash. No external assets so it works
// with no internet and renders instantly on a phone.
static const char PORTAL_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>WakeLight</title>
<style>
:root{--bg:#12100e;--card:#1e1a16;--fg:#f3ede4;--mut:#9a8f80;--acc:#ffa94d;--acc2:#ffd43b}
*{box-sizing:border-box;font-family:-apple-system,system-ui,sans-serif}
body{margin:0;background:var(--bg);color:var(--fg)}
.wrap{max-width:640px;margin:0 auto;padding:16px}
h1{font-size:1.4em;margin:8px 0 2px}
h1 span{color:var(--acc)}
.sub{color:var(--mut);font-size:.85em;margin-bottom:14px}
.card{background:var(--card);border-radius:14px;padding:16px;margin-bottom:14px}
.card h2{font-size:1em;margin:0 0 12px;color:var(--acc2)}
.row{display:flex;align-items:center;gap:10px;margin:10px 0}
.row label{flex:1;color:var(--mut)}
input[type=time],input[type=number],select{background:#2a241e;color:var(--fg);border:1px solid #3a322a;border-radius:8px;padding:8px;font-size:1em}
input[type=number]{width:80px}
input[type=range]{flex:2;accent-color:var(--acc)}
button{background:var(--acc);color:#201405;border:0;border-radius:10px;padding:10px 16px;font-size:1em;font-weight:600;cursor:pointer}
button.ghost{background:#2a241e;color:var(--fg)}
button.danger{background:#7d2b2b;color:#ffd9d9}
.days{display:grid;grid-template-columns:repeat(7,1fr);gap:6px}
.day{background:#2a241e;border-radius:10px;padding:8px 4px;text-align:center}
.day .nm{font-size:.75em;color:var(--mut)}
.day input[type=checkbox]{accent-color:var(--acc);transform:scale(1.2);margin:6px 0}
.day input[type=time]{width:100%;padding:4px 2px;font-size:.8em}
.stat{display:flex;justify-content:space-between;color:var(--mut);font-size:.9em;margin:4px 0}
.stat b{color:var(--fg);font-weight:600}
.pill{display:inline-block;padding:2px 10px;border-radius:99px;font-size:.8em;background:#2a241e}
.pill.sunrise{background:#5b3a13;color:var(--acc2)}
.pill.hold{background:#274a27;color:#b8f5b8}
.pill.manual{background:#1d3a52;color:#a8d8ff}
.btns{display:flex;gap:10px;flex-wrap:wrap;margin-top:10px}
.val{min-width:64px;text-align:right;color:var(--fg)}
.note{color:var(--mut);font-size:.8em;margin-top:8px}
#toast{position:fixed;bottom:18px;left:50%;transform:translateX(-50%);background:var(--acc);color:#201405;padding:10px 18px;border-radius:10px;font-weight:600;opacity:0;transition:.3s;pointer-events:none}
</style></head><body><div class="wrap">
<h1>Wake<span>Light</span></h1>
<div class="sub">Neewer PL60C sunrise controller</div>

<div class="card"><h2>Status</h2>
<div class="stat"><span>State</span><span class="pill" id="state">…</span></div>
<div class="stat"><span>Time</span><b id="now">…</b></div>
<div class="stat"><span>Next alarm</span><b id="next">…</b></div>
<div class="stat"><span>DMX packets sent</span><b id="pkts">…</b></div>
<div class="btns">
<button onclick="api('/api/demo',{})">2-min sunrise demo</button>
<button class="ghost" onclick="api('/api/off',{})">Lamp off / resume schedule</button>
</div></div>

<div class="card"><h2>Alarms <span style="color:var(--mut);font-weight:400">— light is at full at this time</span></h2>
<div class="days" id="days"></div>
<div class="row"><label>Sunrise length (min)</label><input type="number" id="ramp" min="1" max="120"></div>
<div class="row"><label>Stay on after alarm (min)</label><input type="number" id="hold" min="0" max="480"></div>
<div class="btns"><button onclick="saveAlarms()">Save alarms</button></div></div>

<div class="card"><h2>Sunrise feel</h2>
<div class="row"><label>Final brightness</label><input type="range" id="flevel" min="5" max="100"><span class="val" id="flevelv"></span></div>
<div class="row"><label>Start colour (K)</label><input type="range" id="scct" min="2500" max="7000" step="50"><span class="val" id="scctv"></span></div>
<div class="row"><label>Final colour (K)</label><input type="range" id="fcct" min="2500" max="7000" step="50"><span class="val" id="fcctv"></span></div>
<div class="btns"><button onclick="saveFeel()">Save</button></div></div>

<div class="card"><h2>Manual control</h2>
<div class="row"><label>Brightness</label><input type="range" id="mlevel" min="0" max="100" value="0"><span class="val" id="mlevelv">0%</span></div>
<div class="row"><label>Colour (K)</label><input type="range" id="mcct" min="2500" max="10000" step="50" value="3200"><span class="val" id="mcctv">3200K</span></div>
<div class="row"><label>Colour (HSI) instead</label><input type="checkbox" id="mhsi" style="transform:scale(1.3)"></div>
<div class="row"><label>Hue</label><input type="range" id="mhue" min="0" max="360" value="30"><span class="val" id="mhuev">30°</span></div>
<div class="row"><label>Saturation</label><input type="range" id="msat" min="0" max="100" value="100"><span class="val" id="msatv">100%</span></div>
<div class="note">Moving a slider takes the lamp out of schedule mode. “Lamp off / resume schedule” gives it back.</div></div>

<div class="card"><h2>Fixture &amp; system</h2>
<div class="row"><label>DMX address (set on lamp)</label><input type="number" id="addr" min="1" max="512"></div>
<div class="row"><label>Fixture profile</label><select id="mode">
<option value="0">Neewer PL60C (native)</option>
<option value="1">Generic dim+CCT</option>
<option value="2">Custom offsets</option></select></div>
<div class="row"><label>Lamp CCT range (K)</label>
<input type="number" id="kmin" min="1800" max="6500" style="width:70px">–<input type="number" id="kmax" min="3200" max="20000" style="width:70px"></div>
<div class="btns"><button onclick="saveFixture()">Save</button>
<button class="danger" onclick="if(confirm('Forget Wi-Fi and reboot into setup mode?'))api('/api/wifireset',{})">Reset Wi-Fi</button></div></div>

<div id="toast"></div>
<script>
const $=id=>document.getElementById(id);
const DN=['Sun','Mon','Tue','Wed','Thu','Fri','Sat'];
let cfgCache=null;
function toast(m){const t=$('toast');t.textContent=m;t.style.opacity=1;setTimeout(()=>t.style.opacity=0,1600)}
async function api(p,b){const r=await fetch(p,b?{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(b)}:{});if(b)toast(r.ok?'Saved':'Error');return r.ok?r.json().catch(()=>({})):{}}
function buildDays(){$('days').innerHTML=DN.map((n,i)=>`<div class="day"><div class="nm">${n}</div><input type="checkbox" id="en${i}"><input type="time" id="tm${i}"></div>`).join('')}
function fmt(){['flevel','mlevel','msat'].forEach(i=>$(i+'v').textContent=$(i).value+'%');['scct','fcct','mcct'].forEach(i=>$(i+'v').textContent=$(i).value+'K');$('mhuev').textContent=$('mhue').value+'°'}
async function loadCfg(){const c=await api('/api/config');cfgCache=c;
for(let i=0;i<7;i++){$('en'+i).checked=c.alarms[i].on;$('tm'+i).value=String(c.alarms[i].h).padStart(2,'0')+':'+String(c.alarms[i].m).padStart(2,'0')}
$('ramp').value=c.ramp;$('hold').value=c.hold;$('flevel').value=c.flevel;$('scct').value=c.scct;$('fcct').value=c.fcct;
$('addr').value=c.addr;$('mode').value=c.mode;$('kmin').value=c.kmin;$('kmax').value=c.kmax;fmt()}
function alarmsBody(){const a=[];for(let i=0;i<7;i++){const[t,h]=[$('tm'+i).value||'07:00',0];const[hh,mm]=t.split(':');a.push({on:$('en'+i).checked,h:+hh,m:+mm})}return a}
function saveAlarms(){api('/api/config',{alarms:alarmsBody(),ramp:+$('ramp').value,hold:+$('hold').value})}
function saveFeel(){api('/api/config',{flevel:+$('flevel').value,scct:+$('scct').value,fcct:+$('fcct').value})}
function saveFixture(){api('/api/config',{addr:+$('addr').value,mode:+$('mode').value,kmin:+$('kmin').value,kmax:+$('kmax').value})}
let manTimer=null;
function manual(){clearTimeout(manTimer);manTimer=setTimeout(()=>api('/api/manual',{level:+$('mlevel').value,cct:+$('mcct').value,hsi:$('mhsi').checked,hue:+$('mhue').value,sat:+$('msat').value}),120);fmt()}
['mlevel','mcct','mhue','msat'].forEach(i=>$(i).addEventListener('input',manual));
$('mhsi').addEventListener('change',manual);
['flevel','scct','fcct'].forEach(i=>$(i).addEventListener('input',fmt));
async function poll(){try{const s=await api('/api/state');
const el=$('state');el.textContent=s.state;el.className='pill '+s.state.toLowerCase();
$('now').textContent=s.time;$('next').textContent=s.next;$('pkts').textContent=s.pkts.toLocaleString()}catch(e){}}
buildDays();loadCfg();poll();setInterval(poll,2000);
</script></div></body></html>
)HTML";
