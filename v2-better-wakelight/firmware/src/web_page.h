#pragma once
// Single-file portal UI, served from flash. No external assets so it works with
// no internet and renders instantly on a phone. Mirrors design/mockups/11-home.
static const char PORTAL_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>WakeLight</title>
<style>
  :root{
    --bg:#f5f0e8;--grad:#ecdfcd;--panel:#fdfaf4;--panel2:#f2ead9;
    --line:#e6dbc7;--line2:#d8ccb5;--text:#2b2620;--muted:#8a7d6b;--faint:#b8ab95;
    --accent:#cc5500;--good:#6a8f3a;--shadow:0 1px 2px rgba(58,43,24,.06),0 6px 18px rgba(58,43,24,.08);
  }
  *{box-sizing:border-box}
  html,body{margin:0;color:var(--text);
    background:radial-gradient(1200px 700px at 10% -10%,var(--grad),var(--bg) 55%) fixed;
    font:16px/1.5 -apple-system,system-ui,"Segoe UI",sans-serif;-webkit-font-smoothing:antialiased}
  .phone{max-width:420px;margin:0 auto;min-height:100vh;
    padding:max(env(safe-area-inset-top),20px) 16px 104px;position:relative}
  .head{display:flex;justify-content:space-between;align-items:center;
    position:sticky;top:0;z-index:6;margin:0 -16px 12px;padding:10px 16px;
    background:rgba(245,240,232,.9);backdrop-filter:saturate(1.3) blur(8px);-webkit-backdrop-filter:saturate(1.3) blur(8px)}
  h1{font:600 23px/1 ui-serif,"Iowan Old Style",Georgia,serif;margin:0}
  h1 .m{color:var(--accent);font-style:italic;font-weight:500}
  .head .hr{display:flex;flex-direction:column;align-items:flex-end;gap:5px}
  .lamp{font-size:13px;color:var(--muted);cursor:pointer;user-select:none}
  .peersmenu{position:fixed;top:62px;right:16px;z-index:8;background:var(--panel);border:1px solid var(--line2);
    border-radius:14px;box-shadow:0 10px 30px rgba(58,43,24,.22);padding:6px;min-width:200px;max-width:280px;display:none}
  .peersmenu.show{display:block}
  .peersmenu .ph{font-size:10px;text-transform:uppercase;letter-spacing:.08em;color:var(--muted);font-weight:700;padding:8px 10px 5px}
  .peersmenu .pi{display:flex;justify-content:space-between;align-items:center;gap:10px;padding:9px 10px;border-radius:9px;
    color:var(--text);font-size:14px;font-weight:600;cursor:pointer}
  .peersmenu .pi:hover{background:var(--panel2)}
  .peersmenu .pi.self{color:var(--muted);cursor:default;font-weight:500}
  .peersmenu .pi.self:hover{background:transparent}
  .peersmenu .pi .u{font-size:11px;color:var(--muted);font-weight:500}
  .peersmenu .pi .ar{color:var(--accent);font-weight:700}
  .sync{display:inline-flex;align-items:center;gap:6px;font-size:11.5px;font-weight:600;
    padding:4px 10px;border-radius:999px;border:1px solid var(--line2);background:var(--panel);color:var(--muted);transition:.2s}
  .sync .dot{width:8px;height:8px;border-radius:50%;background:var(--muted);flex:0 0 8px}
  .sync.saved{color:var(--good);border-color:#cbe0b0;background:#f1f6e9}
  .sync.saved .dot{background:var(--good)}
  .sync.saving{color:#9a6a1a;border-color:#f0d9a8;background:#fbf1dd}
  .sync.saving .dot{background:#cc9a2a;animation:syncpulse 1s infinite}
  .sync.error{color:#b64a3a;border-color:#e6c4bc;background:#fbe9e5;cursor:pointer}
  .sync.error .dot{background:#b64a3a}
  @keyframes syncpulse{0%,100%{opacity:1}50%{opacity:.3}}
  .card{background:var(--panel);border:1px solid var(--line);border-radius:18px;padding:18px;
    box-shadow:var(--shadow);margin-bottom:14px}
  .h2{font-size:11.5px;text-transform:uppercase;letter-spacing:.09em;color:var(--muted);font-weight:700;margin:0}
  .panel{display:none}.panel.on{display:block}
  .sw{--w:48px;--h:28px;position:relative;width:var(--w);height:var(--h);flex:0 0 var(--w)}
  .sw input{position:absolute;opacity:0;width:100%;height:100%;margin:0;cursor:pointer}
  .sw .tr{position:absolute;inset:0;background:#d8ccb5;border-radius:99px;transition:.2s}
  .sw .kn{position:absolute;top:3px;left:3px;width:22px;height:22px;background:#fff;border-radius:50%;
    box-shadow:0 1px 3px rgba(0,0,0,.25);transition:.2s}
  .sw input:checked + .tr{background:var(--accent)}
  .sw input:checked + .tr + .kn{transform:translateX(20px)}
  .state{text-align:center;position:relative}
  .glow{width:80px;height:80px;border-radius:50%;margin:8px auto 12px;transition:.4s;
    background:radial-gradient(circle at 50% 40%,#fff3cf,#ffce63 45%,#cc5500 100%);
    box-shadow:0 0 0 9px rgba(255,206,99,.18),0 0 32px rgba(204,85,0,.34)}
  .glow.dim{background:radial-gradient(circle at 50% 40%,#e9e0d0,#cdbfa6 60%,#b3a587);
    box-shadow:0 0 0 9px rgba(180,165,140,.13)}
  .glow.ramp{animation:breathe 1.8s ease-in-out infinite}
  @keyframes breathe{0%,100%{box-shadow:0 0 0 9px rgba(255,206,99,.18),0 0 32px rgba(204,85,0,.34)}
    50%{box-shadow:0 0 0 16px rgba(255,206,99,.10),0 0 44px rgba(204,85,0,.5)}}
  .state .lab{font:600 22px/1.1 ui-serif,Georgia,serif}
  .state .sub{color:var(--muted);font-size:13.5px;margin-top:5px}
  .ch{display:flex;justify-content:space-between;align-items:center}
  .ch .lbl{display:flex;align-items:center;gap:10px}
  .ch .word{font-size:12px;font-weight:700;color:var(--accent)}
  .ch .word.off{color:var(--muted)}
  .scroller{position:relative;display:flex;align-items:center;justify-content:center;gap:2px;height:150px;margin-top:10px}
  .band{position:absolute;left:24px;right:24px;top:50px;height:50px;border-top:1px solid var(--line2);
    border-bottom:1px solid var(--line2);border-radius:10px;background:var(--panel2);pointer-events:none}
  .col{height:150px;overflow-y:scroll;scroll-snap-type:y mandatory;scrollbar-width:none;
    text-align:center;width:78px;padding:50px 0;position:relative;z-index:1}
  .col::-webkit-scrollbar{display:none}
  .col .it{height:50px;line-height:50px;scroll-snap-align:center;
    font:600 34px/50px ui-rounded,system-ui;letter-spacing:-.02em;color:var(--faint);transition:color .15s}
  .col .it.sel{color:var(--text)}
  .colon{font:600 30px/1 ui-rounded,system-ui;color:var(--text);z-index:1}
  .body-dim{transition:.2s}
  .card.disabled .body-dim{opacity:.45;pointer-events:none;filter:grayscale(.4)}
  .lr{display:flex;justify-content:space-between;font-size:13.5px;color:var(--muted);margin:14px 0 8px}
  .lr b{color:var(--text)}
  input[type=range].full{width:100%;accent-color:var(--accent);height:26px}
  input[type=time]{background:var(--panel2);border:1px solid var(--line2);color:var(--text);
    border-radius:10px;padding:10px;font:inherit;font-size:16px;width:100%}
  .hintnote{color:var(--muted);font-size:12px;margin-top:12px}
  .endbtn{display:block;width:100%;margin-top:14px;background:transparent;border:1px solid var(--line2);
    color:var(--text);font:inherit;font-weight:600;font-size:14px;border-radius:12px;padding:13px;cursor:pointer}
  .endbtn.active{background:var(--accent);color:#fff;border-color:var(--accent);box-shadow:0 2px 10px rgba(204,85,0,.22)}
  .endbtn:disabled{opacity:.5}
  .days{display:flex;gap:6px;margin-top:12px}
  .d{font-size:12px;font-weight:700;flex:1;height:36px;border-radius:9px;display:grid;place-items:center;
    background:var(--panel2);color:var(--muted);cursor:pointer;border:1px solid var(--line2)}
  .d.on{background:var(--accent);color:#fff;border-color:var(--accent)}
  .curvewrap{margin-top:6px}
  .curve{display:block;width:100%;height:170px;background:var(--panel2);border:1px solid var(--line);
    border-radius:12px;touch-action:none}
  .curve .axis{stroke:var(--line2);stroke-width:1}
  .curve .grid{stroke:var(--line2);stroke-dasharray:2 5;stroke-width:1}
  .curve .area{fill:var(--accent);opacity:.14}
  .curve .line{fill:none;stroke:var(--accent);stroke-width:2.6;stroke-linejoin:round;stroke-linecap:round}
  .curve .lbl{fill:var(--muted);font:10px -apple-system,system-ui,sans-serif}
  .curve .handle{cursor:ew-resize}.curve .hit{cursor:ew-resize}
  .curve .vguide{stroke:var(--accent);stroke-width:1.5;stroke-dasharray:1 6;stroke-linecap:round;opacity:.5}
  .curve .cdot{fill:#fff;stroke:var(--accent);stroke-width:2.5}
  .curve .cdot.center{fill:var(--accent);stroke:#fff;stroke-width:2.5}
  .curve .handle.locked{cursor:default}.curve .handle.locked .cdot{opacity:.4}.curve .handle.locked .vguide{opacity:.25}
  .curve .dragicon{stroke:var(--accent);stroke-width:1.7;fill:none;stroke-linecap:round;stroke-linejoin:round}
  .curve .tlbl{fill:var(--accent);font-weight:600}
  .curve .tip{fill:#fff;font:700 11px -apple-system,system-ui,sans-serif}
  .curve .tipbg{fill:var(--accent);stroke:var(--accent)}
  .fnrow{display:flex;gap:7px;overflow-x:auto;margin-bottom:10px;padding-bottom:3px;scrollbar-width:none}
  .fnrow::-webkit-scrollbar{display:none}
  .fnrow button{flex:0 0 auto;background:var(--panel2);border:1px solid var(--line2);border-radius:99px;
    padding:7px 13px;font:inherit;font-size:12.5px;font-weight:600;color:var(--text);cursor:pointer;white-space:nowrap}
  .fnrow button.on{background:var(--accent);color:#fff;border-color:var(--accent)}
  .cctramp{display:flex;align-items:center;height:42px;margin-top:8px;border-radius:12px;overflow:hidden;border:1px solid var(--line2)}
  .cctfill{flex:1;height:100%}
  .cctend{position:relative;width:48px;height:100%;border:0;padding:0;cursor:pointer;flex:0 0 48px}
  .cctend::after{content:'edit';position:absolute;bottom:3px;left:0;right:0;text-align:center;font-size:8.5px;
    font-weight:700;letter-spacing:.04em;color:rgba(43,38,32,.55);text-transform:uppercase}
  .cctlabels{display:flex;justify-content:space-between;font-size:12px;color:var(--muted);margin-top:7px;font-weight:600}
  .cctpop{display:none;margin-top:12px;background:var(--panel2);border:1px solid var(--line);border-radius:12px;padding:14px}
  .cctpop.show{display:block}
  .cctpop-head{display:flex;justify-content:space-between;align-items:center;font-size:13px;color:var(--muted);margin-bottom:11px}
  .cctpop-head b{color:var(--text);font-size:16px;font-variant-numeric:tabular-nums}
  .cctscale{position:relative;height:34px;border-radius:99px;border:1px solid var(--line2);cursor:pointer;touch-action:none}
  .cctthumb{position:absolute;top:50%;left:0;width:28px;height:28px;border-radius:50%;background:#fff;
    border:3px solid #fff;outline:2px solid var(--accent);transform:translate(-50%,-50%);
    box-shadow:0 2px 7px rgba(0,0,0,.3);pointer-events:none}
  .cctpresets{display:flex;gap:6px;flex-wrap:wrap;margin-top:12px}
  .cctpresets button{flex:1;min-width:50px;background:var(--panel);border:1px solid var(--line2);border-radius:9px;
    padding:8px 4px;font:inherit;font-size:11px;font-weight:700;color:var(--text);cursor:pointer}
  .cctpresets button.on{background:var(--accent);color:#fff;border-color:var(--accent)}
  .list .item{display:flex;justify-content:space-between;align-items:center;padding:15px 0;border-top:1px solid var(--line);cursor:pointer}
  .list .item:first-child{border-top:0}
  .list .item .l b{display:block;font-size:14.5px}.list .item .l span{font-size:12.5px;color:var(--muted)}
  .list .item .r{color:var(--muted);font-size:13px}
  .list .item.locked{opacity:.5;cursor:default}
  .swatch{width:100%;height:54px;border-radius:12px;border:1px solid var(--line2);margin:4px 0 6px;
    background:#ffd9a6;transition:background .15s}
  .fx{display:grid;grid-template-columns:1fr 1fr;gap:10px}
  .fxc{background:var(--panel2);border:1px solid var(--line);border-radius:13px;padding:14px;text-align:center;cursor:pointer}
  .fxc.on{background:#fbe9d8;border-color:var(--accent)}
  .fxc .ic{font-size:22px}.fxc .nm{font-size:12.5px;font-weight:600;margin-top:6px}
  .fxchip{display:flex;align-items:center;gap:11px;width:100%;text-align:left;background:var(--panel2);
    border:1px solid var(--line);border-radius:11px;padding:11px 13px;font:inherit;font-size:14px;font-weight:600;
    color:var(--text);cursor:pointer;margin-bottom:8px}
  .fxchip.on{background:#fbe9d8;border-color:var(--accent)}
  .fxchip .ic{font-size:18px;flex:0 0 22px}
  .tabs{position:fixed;left:50%;transform:translateX(-50%);bottom:14px;width:min(392px,calc(100% - 24px));
    display:grid;grid-template-columns:repeat(4,1fr);background:var(--panel);border:1px solid var(--line2);
    border-radius:20px;box-shadow:0 8px 28px rgba(58,43,24,.18);padding:6px;z-index:5}
  .tab{background:transparent;border:0;font:inherit;font-size:10.5px;font-weight:600;color:var(--muted);
    padding:8px 0 6px;border-radius:14px;cursor:pointer;text-align:center}
  .tab .ic{display:block;font-size:20px;margin-bottom:2px}
  .tab.on{color:var(--accent);background:var(--panel2)}
  .toast{position:fixed;left:50%;bottom:84px;transform:translateX(-50%);background:var(--text);color:var(--panel);
    padding:9px 16px;border-radius:999px;font-size:13px;opacity:0;transition:.2s;pointer-events:none;z-index:9}
  .toast.on{opacity:1}
</style>
</head>
<body>
<div class="phone">
  <div class="head"><h1>Wake<span class="m">light</span></h1>
    <div class="hr"><span class="lamp" id="lampName">…</span>
      <div class="sync saved" id="sync" title="settings sync status"><span class="dot"></span><span id="syncText">Saved</span></div></div></div>
  <div class="peersmenu" id="peers"></div>

  <section class="panel on" data-p="home">
    <div class="card state">
      <div class="glow dim" id="glow"></div>
      <div class="lab" id="stateLab">…</div>
      <div class="sub" id="stateSub"></div>
    </div>
    <div class="card">
      <div class="ch"><p class="h2">Wake-up alarm</p>
        <div class="lbl"><span class="word" id="wakeWord">On</span>
          <label class="sw"><input type="checkbox" id="wakeSw"><span class="tr"></span><span class="kn"></span></label></div></div>
      <div class="scroller" id="scroller"><div class="band"></div>
        <div class="col" id="hCol"></div><div class="colon">:</div><div class="col" id="mCol"></div></div>
      <button class="endbtn" id="endToday">End today’s wake-up</button>
      <p class="hintnote">Stops a wake-up that’s running now (or skips today’s) — the alarm stays on for tomorrow.</p>
    </div>
    <div class="card disabled" id="manualCard">
      <div class="ch"><p class="h2">Turn the light on now</p>
        <div class="lbl"><span class="word off" id="onWord">Off</span>
          <label class="sw"><input type="checkbox" id="onSw"><span class="tr"></span><span class="kn"></span></label></div></div>
      <div class="body-dim">
        <div class="lr"><span>Brightness</span><b id="homeBv">65%</b></div>
        <input type="range" class="full" id="homeB" min="0" max="100" value="65">
        <div class="lr"><span>Warmth</span><b id="homeCv">3000K</b></div>
        <input type="range" class="full" id="homeC" min="2500" max="10000" step="50" value="3000">
        <div class="hintnote">Overrides the schedule until you switch it off. Full colour &amp; effects live in the Manual tab.</div>
      </div>
    </div>
  </section>

  <section class="panel" data-p="sched">
    <div class="card curvewrap">
      <p class="h2" style="margin-bottom:8px">Sunrise shape</p>
      <div class="fnrow" id="fnrow"></div>
      <svg class="curve" id="curve" viewBox="0 0 360 170" aria-hidden="true"></svg>
      <p class="hintnote" style="margin-top:8px">Pick a shape. The <b>outer</b> handles pin where the rise starts &amp; finishes. Drag the <b>middle</b> handle (locked at 50%) to <b>skew</b> the curve. (Linear has nothing to skew.)</p>
      <p class="h2" style="margin:18px 0 8px">Colour through the ramp</p>
      <div class="cctramp">
        <button class="cctend" id="cctStartSw" type="button"></button>
        <div class="cctfill" id="cctbar"></div>
        <button class="cctend" id="cctEndSw" type="button"></button>
      </div>
      <div class="cctlabels"><span id="cctStartV">Start</span><span id="cctEndV">End</span></div>
      <div class="cctpop" id="cctpop">
        <div class="cctpop-head"><span id="cctpopTitle">Start colour temperature</span><b id="cctpopK">2500K</b></div>
        <div class="cctscale" id="cctscale"><div class="cctthumb" id="cctthumb"></div></div>
        <div class="cctpresets" id="cctpresets"></div>
        <p class="hintnote" style="margin-top:10px">Only colour temperatures the PL60C can make — 2500&nbsp;K (warm) to 10000&nbsp;K (cool).</p>
      </div>
    </div>

    <div class="card"><p class="h2">Repeat</p><div class="days" id="days1"></div></div>

    <div class="card">
      <div class="lr" style="margin-top:0"><span>Sunrise length</span><b id="sunlenV">30 min</b></div>
      <input type="range" class="full" id="sunlen" min="5" max="90" value="30">
      <div class="lr"><span>Final brightness</span><b id="finV">100%</b></div>
      <input type="range" class="full" id="finlevel" min="5" max="100" value="100">
      <div class="lr"><span>Stay on after wake</span><b id="holdV">15 min</b></div>
      <input type="range" class="full" id="hold" min="0" max="120" value="15">
    </div>

    <div class="card">
      <div class="ch"><p class="h2">Second alarm</p>
        <div class="lbl"><span class="word off" id="a2word">Off</span>
          <label class="sw"><input type="checkbox" id="a2sw"><span class="tr"></span><span class="kn"></span></label></div></div>
      <div class="body-dim" id="a2body" style="margin-top:10px">
        <div class="lr" style="margin-top:0"><span>Wake time</span></div>
        <input type="time" id="a2time" value="08:30">
        <p class="h2" style="margin-top:14px">Repeat</p>
        <div class="days" id="days2"></div>
      </div>
    </div>
  </section>

  <section class="panel" data-p="man">
    <div class="card disabled" id="manualLightCard">
      <div class="ch"><p class="h2">Light</p>
        <div class="lbl"><span class="word off" id="onWord2">Off</span>
          <label class="sw"><input type="checkbox" id="onSw2"><span class="tr"></span><span class="kn"></span></label></div></div>
      <div class="body-dim">
        <div class="swatch" id="swatch"></div>
        <div class="lr"><span>Brightness</span><b id="mbv">70%</b></div>
        <input type="range" class="full" id="mb" min="0" max="100" value="70">
        <div class="ch" style="margin-top:16px"><span class="lr" style="margin:0">Colour mode</span>
          <div class="lbl"><span class="word off" id="hsiWord">White (CCT)</span>
            <label class="sw"><input type="checkbox" id="hsiSw"><span class="tr"></span><span class="kn"></span></label></div></div>
        <div id="cctMode">
          <div class="lr"><span>Warmth</span><b id="mcctv">3200K</b></div>
          <input type="range" class="full" id="mcct" min="2500" max="10000" step="50" value="3200">
          <div class="lr"><span>Green ↔ Magenta</span><b id="mgmv">0</b></div>
          <input type="range" class="full" id="mgm" min="-50" max="50" value="0">
        </div>
        <div id="hsiModeBox" style="display:none">
          <div class="lr"><span>Hue</span><b id="mhuev">30°</b></div>
          <input type="range" class="full" id="mhue" min="0" max="360" value="30">
          <div class="lr"><span>Saturation</span><b id="msatv">100%</b></div>
          <input type="range" class="full" id="msat" min="0" max="100" value="100">
        </div>
      </div>
    </div>
    <div class="card">
      <p class="h2">Effects</p>
      <div class="fx" id="fxPopular" style="margin-top:12px"></div>
      <button class="endbtn" id="fxToggle" style="margin-top:12px">Show all effects (17)</button>
      <div id="fxAll" style="display:none;margin-top:12px"></div>
      <p class="hintnote">Tap an effect to run it on the lamp.</p>
    </div>
  </section>

  <section class="panel" data-p="set">
    <div class="card list">
      <p class="h2">This lamp</p>
      <div class="item" id="setName"><div class="l"><b>Name</b><span>shown across your network</span></div><div class="r"><span id="nameV">…</span> ›</div></div>
      <div class="item locked"><div class="l"><b>Address</b><span>reach it on Wi-Fi</span></div><div class="r" id="hostV">…</div></div>
    </div>
    <div class="card list">
      <p class="h2">Fixture</p>
      <div class="item" id="setAddr"><div class="l"><b>DMX address</b><span>match the number set on the lamp</span></div><div class="r"><span id="addrV">1</span> ›</div></div>
      <div class="item locked"><div class="l"><b>Fixture profile</b><span>fixed to this board for now</span></div><div class="r">Neewer PL60C</div></div>
      <div class="item locked"><div class="l"><b>Colour range</b><span>warmest–coolest it can do</span></div><div class="r" id="rangeV">2500–10000K</div></div>
    </div>
    <div class="card list">
      <p class="h2">System</p>
      <div class="item locked"><div class="l"><b>Time &amp; location</b><span>sunrise timing &amp; DST</span></div><div class="r">Auto</div></div>
      <div class="item" id="setWifi"><div class="l"><b>Wi-Fi</b><span>forget network &amp; reboot to setup</span></div><div class="r">Reset ›</div></div>
    </div>
  </section>

  <nav class="tabs" id="tabs">
    <button class="tab on" data-t="home"><span class="ic">🏠</span>Home</button>
    <button class="tab" data-t="sched"><span class="ic">📈</span>Schedule</button>
    <button class="tab" data-t="man"><span class="ic">🎛️</span>Manual</button>
    <button class="tab" data-t="set"><span class="ic">⚙️</span>Settings</button>
  </nav>
</div>
<div class="toast" id="toast"></div>

<script>
const $=s=>document.querySelector(s),$$=s=>document.querySelectorAll(s);
const J={'Content-Type':'application/json'};
const api=(p,o)=>fetch(p,o).then(r=>r.ok?r.json().catch(()=>({})):Promise.reject(r));
function toast(m){const t=$('#toast');t.textContent=m;t.classList.add('on');clearTimeout(toast._t);toast._t=setTimeout(()=>t.classList.remove('on'),1400);}

// settings-sync indicator: shows whether changes have reached the lamp
function setSync(s){const el=$('#sync');if(!el)return;el.className='sync '+s;
  $('#syncText').textContent=s==='saving'?'Saving…':s==='error'?'Not saved':'Saved';}
$('#sync').addEventListener('click',()=>{if($('#sync').classList.contains('error'))saveSched();});
// PUT a settings object, reflecting progress in the sync chip
function putJSON(url,body){setSync('saving');
  return api(url,{method:'PUT',headers:J,body:JSON.stringify(body)})
    .then(j=>{setSync('saved');return j;}).catch(e=>{setSync('error');throw e;});}

let sched=null;
let saveT=null;
function saveSched(){clearTimeout(saveT);setSync('saving');saveT=setTimeout(()=>{
  api('/api/schedule',{method:'PUT',headers:J,body:JSON.stringify(sched)})
    .then(j=>{sched=j;setSync('saved');}).catch(()=>setSync('error'));
},350);}

// ---------- tabs ----------
$$('.tab').forEach(t=>t.onclick=()=>{
  $$('.tab').forEach(x=>x.classList.remove('on'));t.classList.add('on');
  $$('.panel').forEach(p=>p.classList.toggle('on',p.dataset.p===t.dataset.t));
  window.scrollTo(0,0);
});

// ---------- effects (id, label, emoji, DMX effect-select byte, default params) ----------
const EFFECTS=[
  {id:'candlelight',n:'Candle',e:'🕯️',pop:1,b:146,p:[255,128,128,50,50,60]},
  {id:'lightning',n:'Lightning',e:'⚡',pop:1,b:7,p:[128,50,60,0,0,0]},
  {id:'tv_screen',n:'TV screen',e:'📺',pop:1,b:202,p:[255,128,128,50,60,0]},
  {id:'fireworks',n:'Fireworks',e:'🎆',pop:1,b:216,p:[100,50,50,60,0,0]},
  {id:'cop_car',n:'Cop car',e:'🚓',pop:1,b:132,p:[40,50,60,0,0,0]},
  {id:'party',n:'Party',e:'🎉',pop:1,b:240,p:[100,50,60,0,0,0]},
  {id:'paparazzi',n:'Paparazzi',e:'📸',b:20,p:[128,128,50,60,0,0]},
  {id:'defective_bulb',n:'Defective bulb',e:'💡',b:34,p:[128,128,50,60,0,0]},
  {id:'explosion',n:'Explosion',e:'💥',b:48,p:[128,128,50,50,60,0]},
  {id:'welding',n:'Welding',e:'🔩',b:62,p:[255,128,128,50,60,0]},
  {id:'cct_flash',n:'CCT flash',e:'🔆',b:76,p:[128,128,50,60,0,0]},
  {id:'hue_flash',n:'Hue flash',e:'🌈',b:90,p:[0,255,50,60,0,0]},
  {id:'cct_pulse',n:'CCT pulse',e:'🔅',b:104,p:[128,128,50,60,0,0]},
  {id:'hue_pulse',n:'Hue pulse',e:'🎨',b:118,p:[0,255,50,60,0,0]},
  {id:'hue_loop',n:'Hue loop',e:'🔁',b:160,p:[0,128,50,60,0,0]},
  {id:'cct_loop',n:'CCT loop',e:'♻️',b:174,p:[128,200,50,60,0,0]},
  {id:'int_loop',n:'Intensity loop',e:'🔄',b:188,p:[0,255,128,50,60,0]},
];
function selectFx(id){
  const f=EFFECTS.find(x=>x.id===id); if(!f)return;
  $$('[data-fx]').forEach(x=>x.classList.toggle('on',x.dataset.fx===id));
  api('/api/effect',{method:'POST',headers:J,body:JSON.stringify({fx:f.b,level:80,p:f.p})});
  setOverrideUI(true);
}
$('#fxPopular').innerHTML=EFFECTS.filter(f=>f.pop).map(f=>`<div class="fxc" data-fx="${f.id}"><div class="ic">${f.e}</div><div class="nm">${f.n}</div></div>`).join('');
$('#fxAll').innerHTML=EFFECTS.map(f=>`<button class="fxchip" data-fx="${f.id}"><span class="ic">${f.e}</span>${f.n}</button>`).join('');
$$('[data-fx]').forEach(el=>el.addEventListener('click',()=>selectFx(el.dataset.fx)));
$('#fxToggle').addEventListener('click',()=>{const open=$('#fxAll').style.display!=='none';
  $('#fxAll').style.display=open?'none':'block';$('#fxToggle').textContent=open?'Show all effects (17)':'Hide full list';});

// ---------- override (linked Home + Manual toggles) ----------
let overrideOn=false;
function setOverrideUI(v){
  overrideOn=v;
  $('#onSw').checked=v; $('#onSw2').checked=v;
  $('#manualCard').classList.toggle('disabled',!v);
  $('#manualLightCard').classList.toggle('disabled',!v);
  $('#onWord').textContent=v?'On':'Off'; $('#onWord').classList.toggle('off',!v);
  $('#onWord2').textContent=v?'On':'Off'; $('#onWord2').classList.toggle('off',!v);
}
function sendOverride(on){ api('/api/override',{method:'POST',headers:J,body:JSON.stringify({on})}); setOverrideUI(on); }
$('#onSw').addEventListener('change',e=>sendOverride(e.target.checked));
$('#onSw2').addEventListener('change',e=>sendOverride(e.target.checked));

// ---------- manual / home light controls ----------
let mT=null;
function sendManual(){clearTimeout(mT);mT=setTimeout(()=>{
  const hsi=$('#hsiSw').checked;
  api('/api/manual',{method:'POST',headers:J,body:JSON.stringify({
    level:+$('#mb').value, hsi, cct:+$('#mcct').value, gm:+$('#mgm').value,
    hue:+$('#mhue').value, sat:+$('#msat').value })});
  setOverrideUI(true);
},120);}
function kToRgb(k){const t=Math.max(0,Math.min(1,(k-2500)/7500));return `rgb(255,${Math.round(180+60*t)},${Math.round(120+135*t)})`;}
function updManual(){
  $('#mbv').textContent=$('#mb').value+'%';$('#mcctv').textContent=$('#mcct').value+'K';
  $('#mgmv').textContent=($('#mgm').value>0?'+':'')+$('#mgm').value;
  $('#mhuev').textContent=$('#mhue').value+'°';$('#msatv').textContent=$('#msat').value+'%';
  const hsi=$('#hsiSw').checked, lum=30+($('#mb').value/100)*55;
  $('#swatch').style.background=hsi?`hsl(${$('#mhue').value} ${$('#msat').value}% ${lum}%)`:kToRgb(+$('#mcct').value);
  $('#swatch').style.opacity=0.35+0.65*($('#mb').value/100);
}
['mb','mcct','mgm','mhue','msat'].forEach(id=>$('#'+id).addEventListener('input',()=>{updManual();sendManual();}));
$('#hsiSw').addEventListener('change',()=>{
  $('#cctMode').style.display=$('#hsiSw').checked?'none':'block';
  $('#hsiModeBox').style.display=$('#hsiSw').checked?'block':'none';
  $('#hsiWord').textContent=$('#hsiSw').checked?'Colour (HSI)':'White (CCT)';
  $('#hsiWord').classList.toggle('off',!$('#hsiSw').checked); updManual(); sendManual();
});
let hmT=null;
function sendHome(){clearTimeout(hmT);hmT=setTimeout(()=>{
  api('/api/manual',{method:'POST',headers:J,body:JSON.stringify({level:+$('#homeB').value,cct:+$('#homeC').value})});
  setOverrideUI(true);
},120);}
$('#homeB').addEventListener('input',()=>{$('#homeBv').textContent=$('#homeB').value+'%';sendHome();});
$('#homeC').addEventListener('input',()=>{$('#homeCv').textContent=$('#homeC').value+'K';sendHome();});

// ---------- wake alarm + scroller ----------
const hCol=$('#hCol'),mCol=$('#mCol');
for(let h=0;h<24;h++)hCol.innerHTML+=`<div class="it">${String(h).padStart(2,'0')}</div>`;
for(let m=0;m<60;m+=5)mCol.innerHTML+=`<div class="it">${String(m).padStart(2,'0')}</div>`;
const selOf=c=>Math.round(c.scrollTop/50);
function markSel(c){const i=selOf(c);[...c.children].forEach((x,k)=>x.classList.toggle('sel',k===i));}
function scrollWake(){return selOf(hCol)*60 + selOf(mCol)*5;}
[hCol,mCol].forEach(c=>{let to;c.addEventListener('scroll',()=>{markSel(c);
  clearTimeout(to);to=setTimeout(()=>{ if(sched){sched.alarms[0].wake=scrollWake(); saveSched(); refreshSub();} },150);});});
$('#wakeSw').addEventListener('change',e=>{
  sched.scheduleOn=e.target.checked;
  $('#wakeWord').textContent=e.target.checked?'On':'Off';$('#wakeWord').classList.toggle('off',!e.target.checked);
  saveSched();
});
$('#endToday').addEventListener('click',()=>{api('/api/dismiss',{method:'POST'}).then(()=>{toast('Skipped today');});});

// ---------- schedule detail sliders ----------
$('#sunlen').addEventListener('input',()=>{$('#sunlenV').textContent=$('#sunlen').value+' min';sched.sunriseMin=+$('#sunlen').value;saveSched();refreshSub();});
$('#finlevel').addEventListener('input',()=>{$('#finV').textContent=$('#finlevel').value+'%';sched.finalLevel=+$('#finlevel').value;saveSched();});
$('#hold').addEventListener('input',()=>{$('#holdV').textContent=$('#hold').value+' min';sched.hold=+$('#hold').value;saveSched();});

// ---------- repeat day pills (Mon-first display -> tm_wday bits) ----------
const DAYBITS=[1,2,3,4,5,6,0], DAYLBL=['M','T','W','T','F','S','S'];
function buildDays(el,ai){
  el.innerHTML=DAYBITS.map((b,i)=>`<div class="d" data-bit="${b}">${DAYLBL[i]}</div>`).join('');
  el.querySelectorAll('.d').forEach(d=>d.onclick=()=>{
    const bit=+d.dataset.bit; sched.alarms[ai].days^=(1<<bit);
    d.classList.toggle('on'); saveSched();
  });
}
function paintDays(el,mask){el.querySelectorAll('.d').forEach(d=>d.classList.toggle('on',!!(mask&(1<<+d.dataset.bit))));}
buildDays($('#days1'),0); buildDays($('#days2'),1);

// ---------- second alarm ----------
$('#a2sw').addEventListener('change',e=>{
  sched.alarms[1].on=e.target.checked;
  $('#a2word').textContent=e.target.checked?'On':'Off';$('#a2word').classList.toggle('off',!e.target.checked);
  $('#a2body').parentElement.classList.toggle('disabled',!e.target.checked);
  saveSched();
});
$('#a2time').addEventListener('change',()=>{const[h,m]=$('#a2time').value.split(':').map(Number);sched.alarms[1].wake=h*60+m;saveSched();});

// ---------- sunrise curve (function + skew warp) ----------
const curve=$('#curve');
const PL=30,PR=336,PT=14,PB=140;
const nx=t=>PL+t*(PR-PL), Y=b=>PB-Math.max(0,Math.min(1,b))*(PB-PT);
let winStartMin=0, wakeMin=420;
function txTime(t){const m=Math.round(winStartMin+Math.max(0,Math.min(1,t))*(wakeMin-winStartMin));
  return String(Math.floor(((m%1440)+1440)%1440/60)).padStart(2,'0')+':'+String(((m%60)+60)%60).padStart(2,'0');}
const FUNCS={
  0:u=>{const k=9,L=x=>1/(1+Math.exp(-k*(x-0.5)));return (L(u)-L(0))/(L(1)-L(0));},
  1:u=>u, 2:u=>Math.pow(u,2.2), 3:u=>1-Math.pow(1-u,2.2),
  4:u=>{const a=3.2;return (Math.exp(a*u)-1)/(Math.exp(a)-1);},
};
const FNAMES=['Sigmoid','Linear','Ease-in','Ease-out','Exponential'];
function invF(f,b){let lo=0,hi=1;for(let i=0;i<40;i++){const m=(lo+hi)/2;if(f(m)<b)lo=m;else hi=m;}return (lo+hi)/2;}
function pchip(xs,ys){const n=xs.length,h=[],d=[];
  for(let i=0;i<n-1;i++){h[i]=xs[i+1]-xs[i];d[i]=(ys[i+1]-ys[i])/h[i];}
  const m=new Array(n);m[0]=d[0];m[n-1]=d[n-2];
  for(let i=1;i<n-1;i++){if(d[i-1]*d[i]<=0)m[i]=0;else{const w1=2*h[i]+h[i-1],w2=h[i]+2*h[i-1];m[i]=(w1+w2)/(w1/d[i-1]+w2/d[i]);}}
  return x=>{let i=n-2;for(let k=0;k<n-1;k++){if(x<=xs[k+1]){i=k;break;}}
    const t=(x-xs[i])/h[i],t2=t*t,t3=t2*t,h00=2*t3-3*t2+1,h10=t3-2*t2+t,h01=-2*t3+3*t2,h11=t3-t2;
    return h00*ys[i]+h10*h[i]*m[i]+h01*ys[i+1]+h11*h[i]*m[i+1];};}
let fnId=0,t0=0,t1=1,sc=0.5,dragH=null,_w=t=>t;
const isLinear=()=>fnId===1;
function median(){return isLinear()?0.5:invF(FUNCS[fnId],0.5);}
function rebuild(){const u0=median(),s=Math.max(0.06,Math.min(0.94,sc));_w=pchip([0,s,1],[0,u0,1]);}
function shapeB(t){const s=(t-t0)/(t1-t0);return s<=0?0:s>=1?1:FUNCS[fnId](_w(s));}
function setFn(id){fnId=id;sc=median();pushCurve();render();}
function pushCurve(){if(!sched)return;sched.curveFn=fnId;sched.t0=Math.round(t0*100);sched.t1=Math.round(t1*100);sched.skew=Math.round(sc*100);saveSched();}
function render(){
  rebuild();
  let line='';for(let i=0;i<=80;i++){const t=i/80;line+=(i?'L':'M')+nx(t).toFixed(1)+','+Y(shapeB(t)).toFixed(1)+' ';}
  const area=line+`L${nx(1).toFixed(1)},${PB} L${nx(0).toFixed(1)},${PB} Z`;
  let grid='';[0,.25,.5,.75,1].forEach(b=>{grid+=`<line class="grid" x1="${PL}" y1="${Y(b).toFixed(1)}" x2="${PR}" y2="${Y(b).toFixed(1)}"/>`+
    `<text class="lbl" x="3" y="${(Y(b)+3).toFixed(1)}" text-anchor="start">${Math.round(b*100)}%</text>`;});
  const tc=t0+sc*(t1-t0);
  const H=[{h:'start',t:t0,b:0,r:6,ic:-17},{h:'center',t:tc,b:0.5,r:7.5,ic:19,center:1,locked:isLinear()},{h:'end',t:t1,b:1,r:6,ic:17}];
  let hh='';
  H.forEach(o=>{const x=nx(o.t),y=Y(o.b);
    hh+=`<g class="handle${o.locked?' locked':''}" data-h="${o.h}">`+
      `<rect class="hit" x="${(x-13).toFixed(1)}" y="${PT}" width="26" height="${(PB-PT).toFixed(1)}" fill="transparent"/>`+
      `<line class="vguide" x1="${x.toFixed(1)}" y1="${PT}" x2="${x.toFixed(1)}" y2="${PB}"/>`+
      `<circle class="cdot${o.center?' center':''}" cx="${x.toFixed(1)}" cy="${y.toFixed(1)}" r="${o.r}"/>`+
      (o.locked?'':`<g class="dragicon" transform="translate(${x.toFixed(1)},${(y+o.ic).toFixed(1)})"><line x1="-6.5" y1="0" x2="6.5" y2="0"/><polyline points="-3.5,-3 -7,0 -3.5,3"/><polyline points="3.5,-3 7,0 3.5,3"/></g>`)+`</g>`;
    if(dragH===o.h){const ty=Math.max(PT+13,y-20);
      hh+=`<rect class="tipbg" x="${(x-22).toFixed(1)}" y="${(ty-12).toFixed(1)}" width="44" height="17" rx="5"/><text class="tip" x="${x.toFixed(1)}" y="${ty.toFixed(1)}" text-anchor="middle">${txTime(o.t)}</text>`;}
  });
  const cands=[{cx:nx(tc),x:nx(tc),txt:txTime(tc),pri:3,a:'middle',c:'lbl tlbl'},
    {cx:nx(t0),x:nx(t0),txt:txTime(t0),pri:2,a:'middle',c:'lbl'},
    {cx:nx(t1),x:nx(t1),txt:txTime(t1),pri:2,a:'middle',c:'lbl'},
    {cx:nx(0)+18,x:nx(0),txt:txTime(0),pri:1,a:'start',c:'lbl'},
    {cx:nx(1)-18,x:nx(1),txt:txTime(1),pri:1,a:'end',c:'lbl'}].sort((a,b)=>b.pri-a.pri);
  const placed=[];let times='';
  cands.forEach(l=>{if(placed.some(p=>Math.abs(p-l.cx)<34))return;placed.push(l.cx);
    times+=`<text class="${l.c}" x="${l.x.toFixed(1)}" y="160" text-anchor="${l.a}">${l.txt}</text>`;});
  curve.innerHTML=`${grid}<line class="axis" x1="${PL}" y1="${PB}" x2="${PR}" y2="${PB}"/><path class="area" d="${area}"/><path class="line" d="${line}"/>${hh}${times}`;
  curve.querySelectorAll('.handle:not(.locked)').forEach(h=>h.addEventListener('pointerdown',startDrag));
  $$('#fnrow button').forEach(b=>b.classList.toggle('on',+b.dataset.fn===fnId));
}
function startDrag(e){
  e.preventDefault();dragH=e.currentTarget.dataset.h;
  const move=ev=>{const r=curve.getBoundingClientRect();
    const scale=Math.min(r.width/360,r.height/170),offX=(r.width-360*scale)/2;
    let nt=Math.max(0,Math.min(1,((ev.clientX-r.left-offX)/scale-PL)/(PR-PL)));
    if(dragH==='center'){sc=Math.max(0.06,Math.min(0.94,(nt-t0)/(t1-t0)));}
    else if(dragH==='start'){t0=Math.max(0,Math.min(nt,t1-0.16));}
    else if(dragH==='end'){t1=Math.min(1,Math.max(nt,t0+0.16));}
    render();};
  const up=()=>{dragH=null;render();pushCurve();removeEventListener('pointermove',move);removeEventListener('pointerup',up);};
  addEventListener('pointermove',move);addEventListener('pointerup',up);
}
$('#fnrow').innerHTML=FNAMES.map((n,i)=>`<button data-fn="${i}">${n}</button>`).join('');
$$('#fnrow button').forEach(b=>b.onclick=()=>setFn(+b.dataset.fn));

// ---------- colour-through-ramp CCT picker (constrained to fixture range) ----------
const KMIN=2500,KMAX=10000,PRESETS=[2500,2700,3200,4000,5000,5600,6500,10000];
let cct={start:2500,end:5600},cctTarget='start';
function cctToRGB(k){k=Math.max(KMIN,Math.min(KMAX,k));const t=k/100;let r,g,b;
  if(t<=66){r=255;g=99.4708025861*Math.log(t)-161.1195681661;b=t<=19?0:138.5177312231*Math.log(t-10)-305.0447927307;}
  else{r=329.698727446*Math.pow(t-60,-0.1332047592);g=288.1221695283*Math.pow(t-60,-0.0755148492);b=255;}
  const cl=x=>Math.max(0,Math.min(255,Math.round(x)));return `rgb(${cl(r)},${cl(g)},${cl(b)})`;}
function locus(k0,k1,n){const s=[];for(let i=0;i<=n;i++){const k=k0+(k1-k0)*i/n;s.push(`${cctToRGB(k)} ${(i/n*100).toFixed(0)}%`);}return `linear-gradient(90deg,${s.join(',')})`;}
function cctDesc(k){return k<2900?'warm':k<4000?'soft white':k<5200?'neutral':k<6800?'daylight':'cool';}
function paintCct(){
  $('#cctStartSw').style.background=cctToRGB(cct.start);$('#cctEndSw').style.background=cctToRGB(cct.end);
  $('#cctbar').style.background=locus(cct.start,cct.end,6);
  $('#cctStartV').textContent=`Start · ${cct.start}K ${cctDesc(cct.start)}`;
  $('#cctEndV').textContent=`${cct.end}K ${cctDesc(cct.end)} · End`;
}
function updThumb(){const k=cct[cctTarget];$('#cctpopK').textContent=k+'K';
  $('#cctthumb').style.left=((k-KMIN)/(KMAX-KMIN)*100)+'%';$('#cctthumb').style.background=cctToRGB(k);
  $$('#cctpresets button').forEach(b=>b.classList.toggle('on',+b.dataset.k===k));}
function setK(k){k=Math.max(KMIN,Math.min(KMAX,Math.round(k/50)*50));cct[cctTarget]=k;updThumb();paintCct();
  if(sched){sched.startCct=cct.start;sched.endCct=cct.end;saveSched();}}
function openCct(w){cctTarget=w;$('#cctpop').classList.add('show');
  $('#cctpopTitle').textContent=(w==='start'?'Start':'End')+' colour temperature';updThumb();}
$('#cctStartSw').addEventListener('click',()=>openCct('start'));
$('#cctEndSw').addEventListener('click',()=>openCct('end'));
const scale=$('#cctscale');scale.style.background=locus(KMIN,KMAX,8);
function scaleK(cx){const r=scale.getBoundingClientRect();return KMIN+Math.max(0,Math.min(1,(cx-r.left)/r.width))*(KMAX-KMIN);}
scale.addEventListener('pointerdown',e=>{setK(scaleK(e.clientX));
  const mv=ev=>setK(scaleK(ev.clientX));const up=()=>{removeEventListener('pointermove',mv);removeEventListener('pointerup',up);};
  addEventListener('pointermove',mv);addEventListener('pointerup',up);});
$('#cctpresets').innerHTML=PRESETS.map(k=>`<button data-k="${k}">${k>=10000?'10k':k+'K'}</button>`).join('');
$$('#cctpresets button').forEach(b=>b.onclick=()=>setK(+b.dataset.k));

// ---------- settings ----------
$('#setName').addEventListener('click',()=>{const v=prompt('Lamp name',$('#nameV').textContent);
  if(v&&v.trim())putJSON('/api/device',{name:v.trim()}).then(j=>{$('#nameV').textContent=j.name;$('#lampName').textContent=j.name+' ▾';}).catch(()=>{});});
$('#setAddr').addEventListener('click',()=>{const v=prompt('DMX address (1-512)',$('#addrV').textContent);
  const n=parseInt(v,10);if(n>=1&&n<=512)putJSON('/api/fixture',{addr:n}).then(j=>$('#addrV').textContent=j.addr).catch(()=>{});});
$('#setWifi').addEventListener('click',()=>{if(confirm('Forget Wi-Fi and reboot into setup mode?'))api('/api/wifireset',{method:'POST'});});

// ---------- peers: list all wakelights on the network ----------
function showPeers(){
  const m=$('#peers');
  if(m.classList.contains('show')){m.classList.remove('show');return;}
  m.innerHTML='<div class="ph">On your network</div><div class="pi self"><span class="u">Scanning…</span></div>';
  m.classList.add('show');
  api('/api/peers').then(j=>{
    const peers=j.peers||[];
    let h='<div class="ph">On your network</div>';
    peers.forEach(p=>{
      if(p.self) h+=`<div class="pi self"><span>${p.name} <span class="u">· this one</span></span></div>`;
      else h+=`<div class="pi" data-url="http://${p.slug}.local"><span>${p.name}<br><span class="u">${p.slug}.local</span></span><span class="ar">→</span></div>`;
    });
    if(peers.filter(p=>!p.self).length===0) h+='<div class="pi self"><span class="u">No other lamps found</span></div>';
    m.innerHTML=h;
    m.querySelectorAll('[data-url]').forEach(el=>el.onclick=()=>{location.href=el.dataset.url;});
  }).catch(()=>{m.innerHTML='<div class="ph">WakeLights</div><div class="pi self"><span class="u">Couldn’t scan</span></div>';});
}
$('#lampName').addEventListener('click',e=>{e.stopPropagation();showPeers();});
document.addEventListener('click',e=>{if(!e.target.closest('#peers')&&!e.target.closest('#lampName'))$('#peers').classList.remove('show');});

// ---------- load + status poll ----------
function setWindow(){ if(!sched)return; wakeMin=sched.alarms[0].wake; winStartMin=wakeMin-sched.sunriseMin; render(); }
function refreshSub(){ setWindow(); }
function applySched(j){
  sched=j;
  $('#wakeSw').checked=j.scheduleOn;$('#wakeWord').textContent=j.scheduleOn?'On':'Off';$('#wakeWord').classList.toggle('off',!j.scheduleOn);
  // scroller
  requestAnimationFrame(()=>{hCol.scrollTop=Math.floor(j.alarms[0].wake/60)*50;mCol.scrollTop=Math.round((j.alarms[0].wake%60)/5)*50;markSel(hCol);markSel(mCol);});
  paintDays($('#days1'),j.alarms[0].days);
  // second alarm
  $('#a2sw').checked=j.alarms[1].on;$('#a2word').textContent=j.alarms[1].on?'On':'Off';$('#a2word').classList.toggle('off',!j.alarms[1].on);
  $('#a2sw').closest('.card').classList.toggle('disabled',!j.alarms[1].on);
  $('#a2time').value=String(Math.floor(j.alarms[1].wake/60)).padStart(2,'0')+':'+String(j.alarms[1].wake%60).padStart(2,'0');
  paintDays($('#days2'),j.alarms[1].days);
  // detail sliders
  $('#sunlen').value=j.sunriseMin;$('#sunlenV').textContent=j.sunriseMin+' min';
  $('#finlevel').value=j.finalLevel;$('#finV').textContent=j.finalLevel+'%';
  $('#hold').value=j.hold;$('#holdV').textContent=j.hold+' min';
  $('#rangeV').textContent=`${j.kmin}–${j.kmax}K`;
  // curve
  fnId=j.curveFn;t0=j.t0/100;t1=j.t1/100;sc=j.skew/100;setWindow();
  // cct
  cct.start=j.startCct;cct.end=j.endCct;paintCct();
}
function statusSub(s){
  if(s.override) return `Manual override · ${s.brightness}% · ${s.cct}K`;
  if(!s.scheduleOn) return 'No wake-up scheduled';
  if(s.label==='Sunrise') return `Ramping · ${s.brightness}% · ${s.cct}K`;
  if(s.label==='Holding') return `Holding at ${s.brightness}%`;
  if(s.label==='Done for today') return 'Done · resumes tomorrow';
  if(!s.time_valid) return 'Waiting for clock…';
  return `Next: ${s.next}`;
}
function poll(){
  api('/api/status').then(s=>{
    $('#stateLab').textContent=s.label;
    $('#stateSub').textContent=statusSub(s);
    const g=$('#glow');g.className='glow'+((s.override||s.label==='Sunrise'||s.label==='Holding')?(s.label==='Sunrise'?' ramp':''):' dim');
    if(overrideOn!==s.override) setOverrideUI(s.override);
    $('#endToday').disabled=!s.scheduleOn;
    $('#endToday').classList.toggle('active',s.label==='Sunrise'||s.label==='Holding');
    $('#endToday').textContent=(s.label==='Sunrise'||s.label==='Holding')?'End this wake-up now':'End today’s wake-up';
  }).catch(()=>{});
}
render(); paintCct();   // draw defaults immediately; data load refines them
Promise.all([
  api('/api/schedule').then(applySched).catch(()=>{}),
  api('/api/device').then(j=>{$('#lampName').textContent=(j.name||'WakeLight')+' ▾';$('#nameV').textContent=j.name;
    $('#hostV').textContent=j.host+'.local'+(j.holds?' · wakelight.local':'');}).catch(()=>{}),
  api('/api/fixture').then(j=>{$('#addrV').textContent=j.addr;$('#rangeV').textContent=`${j.kmin}–${j.kmax}K`;}).catch(()=>{}),
]).then(()=>{updManual();setSync('saved');poll();setInterval(poll,3000);});
</script>
</body>
</html>
)HTML";
