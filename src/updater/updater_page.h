// Upload-Seite des kleinen Updaters (bewusst schlicht, ohne Schriften/Bilder)
#pragma once
#include <pgmspace.h>

static const char UPD_PAGE[] PROGMEM = R"HTML(<!doctype html><html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Firmware-Update</title>
<style>body{font-family:system-ui,sans-serif;background:#111418;color:#e8eaed;margin:0;padding:16px}
.c{max-width:520px;margin:0 auto;background:#1b2027;border:1px solid #2b323b;border-radius:14px;padding:18px}
h1{font-size:20px;margin:0 0 4px}p{color:#9aa4ae;font-size:14px;line-height:1.4}
input,button{font:inherit;padding:10px 12px;border-radius:10px;border:1px solid #2b323b;background:#11161c;color:#e8eaed;width:100%;box-sizing:border-box;margin:6px 0}
button{background:#f3a43b;color:#111;border:0;font-weight:600;cursor:pointer}button.s{background:#2b323b;color:#e8eaed}
button:disabled{opacity:.5}.bar{height:12px;background:#11161c;border-radius:6px;overflow:hidden;margin:10px 0}
.bar div{height:100%;width:0;background:#5bd38a}.m{font-family:monospace;font-size:12px;color:#9aa4ae}</style></head>
<body><div class="c"><h1>Firmware-Update</h1><p id="n" class="m"></p>
<div id="login" style="display:none"><p>Zugangspasswort der Weboberfläche:</p>
<input id="pw" type="password" autocomplete="current-password"><button id="bl">Anmelden</button></div>
<div id="setup" style="display:none"><p>Es ist noch kein eigenes Zugangspasswort vergeben. Bitte zuerst in der normalen
Weboberfläche des Geräts eins festlegen (unten: „Zurück zur Firmware“).</p></div>
<div id="up" style="display:none"><p>Neue Firmware-Datei (<b>firmware.bin</b> des Geräts) auswählen und hochladen.
Danach startet das Gerät mit der neuen Firmware.</p>
<input id="f" type="file" accept=".bin"><button id="bu">Hochladen</button>
<div class="bar"><div id="pr"></div></div></div>
<p id="st"></p><button class="s" id="bb">Zurück zur Firmware (ohne Update)</button></div>
<script>
const $=s=>document.querySelector(s);let INFO={};
const show=(id)=>{['login','setup','up'].forEach(x=>$('#'+x).style.display=x===id?'':'none')};
async function info(){try{INFO=await (await fetch('/api/info')).json();$('#n').textContent=INFO.name+' · '+INFO.ip+' · '+(INFO.fw?'Firmware: '+INFO.ver:'keine gültige Firmware');}catch(e){}}
async function auth(){const a=await (await fetch('/api/auth')).json();show(a.setup?'setup':a.authed?'up':'login')}
$('#bl').onclick=async()=>{const r=await fetch('/api/login',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({pw:$('#pw').value})});
 if(r.ok)auth();else $('#st').textContent='Passwort falsch.'};
$('#bu').onclick=()=>{const f=$('#f').files[0];if(!f)return;if(INFO.slot&&f.size>INFO.slot){$('#st').textContent='Datei zu groß für dieses Gerät.';return}
 if(f.size<200000){$('#st').textContent='Das ist keine vollständige Firmware-Datei.';return}
 const fd=new FormData();fd.append('fw',f);const x=new XMLHttpRequest();x.open('POST','/api/update?size='+f.size);
 x.upload.onprogress=e=>{if(e.lengthComputable)$('#pr').style.width=(100*e.loaded/e.total)+'%'};
 x.onload=()=>{let j={};try{j=JSON.parse(x.responseText)}catch(e){}
  if(x.status===200){$('#st').textContent='Fertig – das Gerät startet mit der neuen Firmware. Die Seite lädt gleich neu.';setTimeout(()=>location.href='/',12000)}
  else if(x.status===401){auth()}else $('#st').textContent='Fehler: '+(j.error||x.status)};
 x.onerror=()=>{$('#st').textContent='Verbindung abgebrochen.'};$('#bu').disabled=true;$('#st').textContent='Lade hoch …';x.send(fd)};
$('#bb').onclick=async()=>{const r=await fetch('/api/back',{method:'POST'});let j={};try{j=await r.json()}catch(e){}
 if(r.ok){$('#st').textContent='Starte die Firmware …';setTimeout(()=>location.href='/',9000)}else if(r.status===401)auth();else $('#st').textContent=j.error||'Fehler'};
info();auth();
</script></body></html>)HTML";
