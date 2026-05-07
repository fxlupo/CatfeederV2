// =============================================================================
// CatFeeder ESP32 – Eingebettetes Webinterface (HTML in PROGMEM)
// =============================================================================
#include "web.h"

// Das gesamte HTML/CSS/JS liegt in PROGMEM, um RAM zu sparen.
static const char INDEX_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html><html lang="de"><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>CatFeeder</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
:root{--bg:#0f1117;--c1:#1a1d2e;--c2:#242838;--ac:#e8564a;--ac2:#3a5bd9;
      --ok:#22c55e;--wn:#eab308;--er:#ef4444;--t1:#f0f0f5;--t2:#8b8fa3;--r:10px}
body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif;
     background:var(--bg);color:var(--t1);min-height:100vh;padding-bottom:70px;
     -webkit-tap-highlight-color:transparent}
.hd{background:linear-gradient(135deg,#1e2a4a,var(--ac));padding:14px 16px;
    text-align:center;position:sticky;top:0;z-index:100;
    box-shadow:0 2px 12px rgba(0,0,0,.4)}
.hd h1{font-size:1.2em;letter-spacing:.5px}
.hd small{color:rgba(255,255,255,.7);font-size:.72em}
.fw{position:absolute;right:12px;top:10px;font-weight:700}
nav{display:flex;background:var(--c1);position:sticky;top:48px;z-index:99;
    border-bottom:1px solid rgba(255,255,255,.06);overflow-x:auto}
nav button{flex:1;padding:10px 6px;background:none;border:none;color:var(--t2);
           font-size:.72em;font-weight:600;cursor:pointer;min-width:65px;
           border-bottom:2px solid transparent;transition:.2s}
nav button.on{color:var(--ac);border-bottom-color:var(--ac)}
.pg{display:none;padding:10px 10px 20px}
.pg.on{display:block}
.cd{background:var(--c1);border-radius:var(--r);padding:14px;margin-bottom:10px}
.cd h3{font-size:.88em;margin-bottom:10px;color:var(--ac);display:flex;align-items:center;gap:5px}
.g2{display:grid;grid-template-columns:1fr 1fr;gap:6px}
.g3{display:grid;grid-template-columns:1fr 1fr 1fr;gap:6px}
.st{background:var(--c2);border-radius:8px;padding:8px;text-align:center}
.st .v{font-size:1.2em;font-weight:700}.st .l{font-size:.65em;color:var(--t2);margin-top:1px}
.bar{height:18px;background:var(--c2);border-radius:9px;overflow:hidden;margin:6px 0}
.bar .f{height:100%;border-radius:9px;transition:width .5s ease;
        background:linear-gradient(90deg,var(--er),var(--wn) 40%,var(--ok))}
.led{display:inline-block;width:8px;height:8px;border-radius:50%;margin-right:3px}
.led.g{background:var(--ok);box-shadow:0 0 5px var(--ok)}
.led.r{background:var(--er);box-shadow:0 0 5px var(--er)}
.sr{display:flex;justify-content:space-between;align-items:center;padding:5px 0;
    border-bottom:1px solid rgba(255,255,255,.04);font-size:.82em}
.sr:last-child{border:none}
input,select{width:100%;padding:9px;border-radius:7px;
             border:1px solid rgba(255,255,255,.1);background:var(--c2);
             color:var(--t1);font-size:.88em}
input:focus,select:focus{outline:none;border-color:var(--ac)}
label{font-size:.75em;color:var(--t2);display:block;margin-top:6px}
.bt{padding:11px 16px;border:none;border-radius:8px;font-size:.88em;font-weight:600;
    cursor:pointer;width:100%;margin:3px 0;transition:transform .1s;-webkit-user-select:none}
.bt:active{transform:scale(.97)}
.b1{background:var(--ac);color:#fff}
.b2{background:var(--ac2);color:#fff}
.b3{background:var(--er);color:#fff}
.bs{padding:7px 10px;font-size:.78em;width:auto}
.sw{position:relative;display:inline-block;width:40px;height:22px;flex-shrink:0}
.sw input{opacity:0;width:0;height:0}
.sw span{position:absolute;cursor:pointer;inset:0;background:rgba(255,255,255,.15);
         border-radius:22px;transition:.25s}
.sw span:before{content:"";position:absolute;height:16px;width:16px;left:3px;bottom:3px;
                background:#fff;border-radius:50%;transition:.25s}
.sw input:checked+span{background:var(--ok)}
.sw input:checked+span:before{transform:translateX(18px)}
.sl{background:var(--c2);border-radius:8px;padding:10px;margin:6px 0;
    display:flex;align-items:center;gap:6px;flex-wrap:wrap}
.sl.off{opacity:.35}
.sl input[type=time]{width:95px;padding:6px}
.sl input[type=number]{width:52px;padding:6px;text-align:center}
.sl select{width:78px;padding:6px}
.rw{display:flex;align-items:center;gap:6px}
.rw input[type=range]{flex:1;accent-color:var(--ac)}
.rw .rv{min-width:32px;text-align:center;font-weight:600;font-size:.85em}
.toast{position:fixed;bottom:16px;left:50%;transform:translateX(-50%);
       padding:10px 22px;border-radius:8px;font-size:.82em;font-weight:600;
       z-index:1000;opacity:0;transition:opacity .25s;pointer-events:none;color:#fff}
.toast.s{opacity:1;background:var(--ok)}
.toast.e{opacity:1;background:var(--er)}
@media(max-width:380px){.g2{grid-template-columns:1fr}.g3{grid-template-columns:1fr 1fr}}
</style></head><body>

<div class="hd"><h1>&#x1F431; CatFeeder</h1>
<small id="fw" class="fw">--</small>
<small id="ht">--:--:--</small> &middot; <small id="hi"></small></div>

<nav>
<button class="on" onclick="pg('da',this)">&#x1F4CA; Status</button>
<button onclick="pg('sc',this)">&#x23F0; Zeiten</button>
<button onclick="pg('ca',this)">&#x1F527; Kalibr.</button>
<button onclick="pg('se',this)">&#x2699; Einst.</button>
</nav>

<!-- ══════════════ DASHBOARD ══════════════ -->
<div id="da" class="pg on">

<div class="cd"><h3>&#x1F35D; Fütterung</h3>
<div class="g2">
<div><label>Menge (g)</label><input type="number" id="fg" value="20" min="5" max="100" step="5"></div>
<div><label>Auslass</label><select id="fs">
<option value="0">Beide</option><option value="1">Servo 1</option><option value="2">Servo 2</option></select></div>
</div>
<button class="bt b1" onclick="feed()" style="margin-top:10px">&#x1F431; Jetzt füttern!</button></div>

<div class="cd"><h3>&#x1F4E6; Füllstand</h3>
<div class="bar"><div class="f" id="fb" style="width:0%"></div></div>
<div style="text-align:center"><span style="font-size:1.5em;font-weight:700" id="fp">--%</span>
<div style="font-size:.7em;color:var(--t2)" id="fm">-- mm</div></div></div>

<div class="cd"><h3>&#x26A1; Strom</h3>
<div class="g3">
<div class="st"><div class="v" id="sv">--</div><div class="l">Volt</div></div>
<div class="st"><div class="v" id="sa">--</div><div class="l">mA</div></div>
<div class="st"><div class="v" id="sw">--</div><div class="l">mW</div></div></div></div>

<div class="cd"><h3>&#x1F4E1; Sensoren</h3>
<div class="sr"><span>INA219</span><span id="li"></span></div>
<div class="sr"><span>VL53L0X</span><span id="lv"></span></div>
<div class="sr"><span>AS5600</span><span id="la"></span></div>
<div class="sr"><span>DS3231 RTC</span><span id="lr"></span></div>
<div class="sr"><span>Encoder</span><span id="ae">--°</span></div></div>

<div class="cd"><h3>&#x1F4A1; IR Sensoren</h3>
<div class="g2">
<div class="st"><div class="v" id="i1a">--</div><div class="l">IR1 Analog</div></div>
<div class="st"><div class="v" id="i2a">--</div><div class="l">IR2 Analog</div></div>
<div class="st"><div class="v" id="i1d">--</div><div class="l">IR1 Digital</div></div>
<div class="st"><div class="v" id="i2d">--</div><div class="l">IR2 Digital</div></div></div></div>

<div class="cd"><h3>&#x2139; System</h3>
<div class="sr"><span>Uptime</span><span id="su">--</span></div>
<div class="sr"><span>Freier RAM</span><span id="sh">--</span></div>
<div class="sr"><span>Fütterungen</span><span id="sfc">--</span></div></div>
</div>

<!-- ══════════════ FÜTTERUNGSZEITEN ══════════════ -->
<div id="sc" class="pg">
<div class="cd"><h3>&#x23F0; Fütterungsplan</h3>
<p style="font-size:.72em;color:var(--t2);margin-bottom:8px">Bis zu 8 Zeiten. Aktiv = wird täglich ausgeführt.</p>
<div id="slc"></div>
<button class="bt b1" onclick="sav()" style="margin-top:10px">&#x1F4BE; Speichern</button></div></div>

<!-- ══════════════ KALIBRIERUNG ══════════════ -->
<div id="ca" class="pg">

<div class="cd"><h3>&#x1F504; Servo 1</h3>
<div class="rw"><span>0°</span>
<input type="range" id="r1" min="0" max="180" value="90"
  oninput="$('r1v').textContent=this.value+'°';tsv(1,+this.value)">
<span class="rv" id="r1v">90°</span><span>180°</span></div>
<div class="g2" style="margin-top:6px">
<button class="bt b2 bs" onclick="cso(1)">&#x1F4CC; = Offen</button>
<button class="bt b2 bs" onclick="csc(1)">&#x1F4CC; = Zu</button>
<button class="bt b2 bs" onclick="svcmd(1,'open')">&#x25B6; Offen fahren</button>
<button class="bt b2 bs" onclick="svcmd(1,'close')">&#x25C0; Zu fahren</button></div></div>

<div class="cd"><h3>&#x1F504; Servo 2</h3>
<div class="rw"><span>0°</span>
<input type="range" id="r2" min="0" max="180" value="90"
  oninput="$('r2v').textContent=this.value+'°';tsv(2,+this.value)">
<span class="rv" id="r2v">90°</span><span>180°</span></div>
<div class="g2" style="margin-top:6px">
<button class="bt b2 bs" onclick="cso(2)">&#x1F4CC; = Offen</button>
<button class="bt b2 bs" onclick="csc(2)">&#x1F4CC; = Zu</button>
<button class="bt b2 bs" onclick="svcmd(2,'open')">&#x25B6; Offen fahren</button>
<button class="bt b2 bs" onclick="svcmd(2,'close')">&#x25C0; Zu fahren</button></div></div>

<div class="cd"><h3>&#x1F3CE; Servo-Geschwindigkeit</h3>
<label>Grad pro Sekunde</label>
<input type="number" id="ss" value="1000" min="20" max="3000" step="50"></div>

<div class="cd"><h3>&#x2699; Stepper</h3>
<div class="g2">
<div><label>Test-Steps</label><input type="number" id="ts" value="200" min="1" max="50000" step="50"></div>
<div><label>Geschw. (Steps/s)</label><input type="number" id="cs" value="1200" min="100" max="10000" step="100"></div>
<div><label>STEP-Puls (µs)</label><input type="number" id="spu" value="10" min="2" max="50" step="1"></div>
<div><label>DIR-Setup (µs)</label><input type="number" id="sds" value="300" min="0" max="2000" step="50"></div>
<div><label>Haltestrom (ms)</label><input type="number" id="shm" value="0" min="0" max="5000" step="100"></div>
<div><label>Richtung invertieren</label><select id="sdi"><option value="0">Nein</option><option value="1">Ja</option></select></div>
<div><label>Blockierstrom (mA)</label><input type="number" id="sbm" value="1500" min="100" max="5000" step="100"></div>
<div style="display:flex;align-items:flex-end;gap:4px">
<button class="bt b2 bs" onclick="tst(1)">&#x25B6; Vor</button>
<button class="bt b2 bs" onclick="tst(-1)">&#x25C0; Zurück</button></div></div></div>

<div class="cd"><h3>&#x2696; Futter</h3>
<label>Steps pro Gramm</label>
<input type="number" id="cg" value="10" min="1" max="200"></div>

<div class="cd"><h3>&#x1F4CF; Füllstand</h3>
<label>Abstand "Leer" (mm)</label><input type="number" id="ce" value="300" min="50" max="600">
<label>Abstand "Voll" (mm)</label><input type="number" id="cf" value="30" min="10" max="200">
<div class="sr" style="margin-top:6px"><span>Aktuell</span><strong id="cd">-- mm</strong></div></div>

<button class="bt b1" onclick="sav()">&#x1F4BE; Alles speichern</button></div>

<!-- ══════════════ EINSTELLUNGEN ══════════════ -->
<div id="se" class="pg">

<div class="cd"><h3>&#x1F4F6; WLAN</h3>
<label>SSID</label><input type="text" id="ws" placeholder="Netzwerkname">
<label>Passwort</label><input type="password" id="wp" placeholder="WLAN-Passwort">
<button class="bt b1" onclick="swf()" style="margin-top:8px">&#x1F4F6; Speichern &amp; Neustart</button></div>

<div class="cd"><h3>&#x1F570; Uhrzeit</h3>
<div class="sr"><span>RTC</span><span id="rt">--:--:--</span></div>
<button class="bt b2" onclick="syn()">&#x1F504; Vom Handy synchronisieren</button></div>

<div class="cd"><h3>&#x1F30D; Zeitzone</h3>
<label>UTC-Offset (h)</label><input type="number" id="tz" value="1" min="-12" max="14">
<label style="display:flex;align-items:center;gap:8px;margin-top:6px">
<label class="sw"><input type="checkbox" id="ds" checked><span></span></label>Sommerzeit</label></div>

<div class="cd"><h3>&#x1F3F7; Hostname</h3>
<label>mDNS-Name</label><input type="text" id="hn" value="catfeeder" maxlength="32"
  oninput="$('hp').textContent=this.value||'catfeeder'">
<small style="color:var(--t2)">http://<span id="hp">catfeeder</span>.local</small></div>

<button class="bt b1" onclick="sav()">&#x1F4BE; Speichern</button>

<div class="cd" style="margin-top:20px;border:1px solid var(--er)">
<h3 style="color:var(--er)">&#x26A0; Werksreset</h3>
<p style="font-size:.75em;color:var(--t2);margin-bottom:8px">Setzt alles auf Standardwerte zurück.</p>
<button class="bt b3" onclick="if(confirm('Wirklich zurücksetzen?'))rst()">&#x1F5D1; Reset</button></div></div>

<div class="toast" id="tt"></div>

<script>
const $=id=>document.getElementById(id);
let C=null;

// ── Navigation ──
function pg(id,btn){
  document.querySelectorAll('.pg').forEach(p=>p.classList.remove('on'));
  document.querySelectorAll('nav button').forEach(b=>b.classList.remove('on'));
  $(id).classList.add('on'); btn.classList.add('on');
}

// ── Toast ──
function msg(t,e){const el=$('tt');el.textContent=t;el.className='toast '+(e?'e':'s');
  setTimeout(()=>el.className='toast',2500);}

// ── API ──
async function api(u,d){
  try{const o=d?{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(d)}:{};
    return await(await fetch(u,o)).json();}
  catch(x){msg('Verbindungsfehler!',1);return null;}}

// ── Config laden ──
async function lc(){
  C=await api('/api/config'); if(!C)return;
  const c=$('slc'); c.innerHTML='';
  for(let i=0;i<C.slots.length;i++){const s=C.slots[i];
    const h=String(s.h).padStart(2,'0'), m=String(s.m).padStart(2,'0');
    c.innerHTML+=`<div class="sl ${s.on?'':'off'}" id="s${i}">
      <label class="sw"><input type="checkbox" ${s.on?'checked':''} onchange="tg(${i},this.checked)"><span></span></label>
      <input type="time" value="${h}:${m}" onchange="ut(${i},this.value)">
      <input type="number" value="${s.g}" min="5" max="100" step="5" onchange="C.slots[${i}].g=+this.value">
      <span style="font-size:.7em;color:var(--t2)">g</span>
      <select onchange="C.slots[${i}].sv=+this.value">
        <option value="0"${s.sv==0?' selected':''}>Beide</option>
        <option value="1"${s.sv==1?' selected':''}>S1</option>
        <option value="2"${s.sv==2?' selected':''}>S2</option></select></div>`;}
  $('fw').textContent=C.fw||'--';
  $('cg').value=C.spg; $('cs').value=C.spd;
  $('spu').value=C.spu||10; $('sds').value=C.sds??300; $('shm').value=C.shm??0;
  $('sdi').value=C.sdi?1:0; $('sbm').value=C.sbm||1500;
  $('ss').value=C.svs||1000;
  $('ce').value=C.feM; $('cf').value=C.ffM;
  $('r1').value=C.s1o; $('r1v').textContent=C.s1o+'°';
  $('r2').value=C.s2o; $('r2v').textContent=C.s2o+'°';
  $('tz').value=C.tz;  $('ds').checked=C.dst;
  $('hn').value=C.hn;  $('hp').textContent=C.hn;
}
function tg(i,v){C.slots[i].on=v;$('s'+i).className='sl '+(v?'':'off');}
function ut(i,v){const p=v.split(':');C.slots[i].h=+p[0];C.slots[i].m=+p[1];}

// ── Config speichern ──
async function sav(){
  C.spg=+$('cg').value; C.spd=+$('cs').value;
  C.spu=+$('spu').value; C.sds=+$('sds').value; C.shm=+$('shm').value;
  C.sdi=$('sdi').value==='1'; C.sbm=+$('sbm').value;
  C.svs=+$('ss').value;
  C.feM=+$('ce').value; C.ffM=+$('cf').value;
  C.tz=+$('tz').value;  C.dst=$('ds').checked;
  C.hn=$('hn').value;
  const r=await api('/api/config',C);
  if(r&&r.ok)msg('Gespeichert ✓');}

// ── Aktionen ──
async function feed(){msg('Fütterung …');
  await api('/api/feed',{g:+$('fg').value,sv:+$('fs').value});}
function tsv(n,a){api('/api/sv',{n,a});}
function cso(n){C['s'+n+'o']=+$('r'+n).value;msg('Servo '+n+' Offen = '+$('r'+n).value+'°');}
function csc(n){C['s'+n+'c']=+$('r'+n).value;msg('Servo '+n+' Zu = '+$('r'+n).value+'°');}
async function svcmd(n,cmd){await sav();await api('/api/sv',{n,cmd});msg('Servo '+n+' '+cmd);}
async function tst(d){await sav();const s=+$('ts').value*d;await api('/api/stp',{s});msg(s+' Steps @ '+$('cs').value+' sps');}
async function syn(){const n=new Date();
  await api('/api/time',{y:n.getFullYear(),mo:n.getMonth()+1,d:n.getDate(),
    h:n.getHours(),mi:n.getMinutes(),s:n.getSeconds()});msg('Zeit synchronisiert ✓');}
async function swf(){const ss=$('ws').value;if(!ss){msg('SSID eingeben!',1);return;}
  if(!confirm('WLAN ändern & neu starten?'))return;
  await api('/api/wifi',{ssid:ss,pw:$('wp').value});msg('Neustart…');}
async function rst(){await api('/api/reset',{});msg('Reset…');}

// ── SSE ──
function sse(){
  const es=new EventSource('/events');
  es.addEventListener('s',e=>{
    const d=JSON.parse(e.data);
    if(d.fw)$('fw').textContent=d.fw;
    $('ht').textContent=d.t||'--:--:--';
    $('fp').textContent=d.fl+'%'; $('fm').textContent=d.mm+' mm';
    $('fb').style.width=d.fl+'%';
    $('sv').textContent=(d.v||0).toFixed(2);
    $('sa').textContent=(d.ma||0).toFixed(0);
    $('sw').textContent=(d.mw||0).toFixed(0);
    $('ae').textContent=(d.ang||0).toFixed(1)+'°';
    if($('cd'))$('cd').textContent=d.mm+' mm';
    const er=d.er||{};
    led('li',!er.i); led('lv',!er.v); led('la',!er.a); led('lr',!er.r);
    const ir=d.ir||{};
    $('i1a').textContent=ir.a1||0; $('i2a').textContent=ir.a2||0;
    $('i1d').textContent=ir.d1?'HIGH':'LOW'; $('i2d').textContent=ir.d2?'HIGH':'LOW';
    $('su').textContent=upt(d.up||0);
    $('sh').textContent=((d.hp||0)/1024).toFixed(1)+' kB';
    $('sfc').textContent=d.fc||0;
    $('rt').textContent=d.t||'--';
  });
  es.onerror=()=>{es.close();setTimeout(sse,5000);};
}
function led(id,on){const e=$(id);if(!e)return;
  e.innerHTML='<span class="led '+(on?'g':'r')+'"></span>'+(on?'OK':'—');}
function upt(s){const d=Math.floor(s/86400),h=Math.floor(s%86400/3600),m=Math.floor(s%3600/60);
  return(d?d+'d ':'')+h+'h '+m+'m';}

$('hi').textContent=location.hostname;
lc(); sse();
</script></body></html>
)rawhtml";

String Web::_html() {
    return String(INDEX_HTML);
}
