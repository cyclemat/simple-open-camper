// Satellit: Weboberflaeche (eine Seite, wird direkt aus dem Flash ausgeliefert)
#pragma once
#include <pgmspace.h>

static const char SAT_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="de"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>PanelSat</title>
<style>
:root{--bg:#0F1317;--card:#181D22;--line:#272E36;--tx:#E8ECEF;--tx2:#9AA5B1;--ac:#3C8DFF;--ok:#5BD38A;--warn:#F3A43B;--err:#FF6A5C}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--tx);font:15px/1.45 system-ui,-apple-system,"Segoe UI",sans-serif}
main{max-width:760px;margin:0 auto;padding:16px}
h1{font-size:22px;margin:4px 0 2px}h2{font-size:16px;margin:0 0 10px}.sub{color:var(--tx2);margin:0 0 14px}
.card{background:var(--card);border:1px solid var(--line);border-radius:16px;padding:16px;margin:0 0 14px}
.chips{display:flex;flex-wrap:wrap;gap:8px;margin:0 0 14px}.chip{border:1px solid var(--line);border-radius:999px;padding:3px 10px;font-size:13px;color:var(--tx2)}
.chip.ok{color:var(--ok);border-color:#2c5a3e}.chip.err{color:var(--err);border-color:#5a2c2c}
label.f{display:block;margin:0 0 10px}label.f span{display:block;font-size:13px;color:var(--tx2);margin:0 0 4px}
input,select{width:100%;background:#0F1317;color:var(--tx);border:1px solid var(--line);border-radius:10px;padding:9px 10px;font:inherit}
input[type=checkbox]{width:auto;margin:0 8px 0 0;transform:scale(1.2)}
.row{display:flex;gap:10px;flex-wrap:wrap}.row>*{flex:1;min-width:140px}
.ck{display:flex;align-items:center;margin:6px 0}
button{background:#232A31;color:var(--tx);border:1px solid var(--line);border-radius:10px;padding:8px 14px;font:inherit;cursor:pointer}
button.p{background:var(--ac);border-color:var(--ac);color:#fff}button.d{color:var(--err)}button:disabled{opacity:.5;cursor:default}
.dev{border-top:1px solid var(--line);padding:10px 0;display:flex;gap:10px;flex-wrap:wrap;align-items:center}
.dev:first-child{border-top:0}.dev .t{flex:1;min-width:180px}.dev .v{color:var(--tx2);font-size:13px}
.hint{color:var(--tx2);font-size:13px;margin:6px 0 10px}.mono{font-family:ui-monospace,monospace}
.io{border:1px solid var(--line);border-radius:12px;padding:12px;margin:0 0 10px}.ioh{display:flex;gap:10px;align-items:center;margin:0 0 8px}.ioh b{flex:1}
.live{color:var(--tx2);font-size:13px}.sec{margin:16px 0 8px;font-size:15px}.pr{display:flex;gap:8px;align-items:flex-end;flex-wrap:wrap}.pr>*{flex:1;min-width:110px}
.pr .lbl{flex:none;width:70px;padding-bottom:18px;color:var(--tx2);font-size:13px}button.s{padding:5px 10px;font-size:13px}
#toast{position:fixed;left:50%;bottom:20px;transform:translateX(-50%);background:#232A31;border:1px solid var(--line);border-radius:12px;padding:10px 16px;display:none;max-width:90%}
</style></head><body><main>
<h1 id="hName">PanelSat</h1><p class="sub" id="hSub">…</p>
<div class="chips" id="chips"></div>

<div class="card"><h2>Satellit</h2>
<div class="row"><label class="f"><span>Name</span><input id="cName" maxlength="32"></label>
<label class="f"><span>Kennung (Teil der Topics)</span><input id="cId" maxlength="32" class="mono"></label></div>
<h2 style="margin-top:6px">WLAN</h2>
<div class="row"><label class="f"><span>Netzname (SSID)</span><input id="cSsid" maxlength="32" list="nets"></label>
<label class="f"><span>Passwort</span><input id="cWpass" type="password" maxlength="64" placeholder="unverändert"></label></div>
<datalist id="nets"></datalist>
<div class="row" style="flex:none"><button id="bScanW" style="flex:none">Netze suchen</button></div>
<p class="hint">Ohne WLAN-Verbindung öffnet der Satellit nach 30 s das WLAN „SOC-Satellite-…“ (Passwort <b>panelsat</b>), Adresse 192.168.4.1.</p>

<h2 style="margin-top:14px">Wege zum Panel</h2>
<label class="ck"><input type="checkbox" id="mMqtt">MQTT-Broker (auch Home Assistant)</label>
<div id="mqttBox" style="margin-left:28px">
<div class="row"><label class="f"><span>Broker</span><input id="qHost" placeholder="192.168.1.20"></label>
<label class="f" style="flex:.4"><span>Port</span><input id="qPort" type="number" min="1" max="65535"></label></div>
<div class="row"><label class="f"><span>Benutzer</span><input id="qUser" autocomplete="off"></label>
<label class="f"><span>Passwort</span><input id="qPass" type="password" placeholder="unverändert" autocomplete="off"></label></div>
<div class="row"><label class="f"><span>Basis-Topic</span><input id="qBase" class="mono"></label>
<label class="f"><span>Home-Assistant-Präfix</span><input id="qHa" class="mono"></label></div>
<label class="ck"><input type="checkbox" id="qHaOn">Home-Assistant-Discovery (Sensoren erscheinen automatisch in HA)</label>
</div>
<label class="ck"><input type="checkbox" id="mUdp">UDP ins WLAN (ohne Broker)</label>
<div id="udpBox" style="margin-left:28px"><label class="f"><span>Panel-IP (leer = an alle im Netz)</span><input id="uIp" placeholder="leer = Broadcast" class="mono"></label></div>
<label class="ck"><input type="checkbox" id="mBle">Bluetooth direkt ans Panel (ohne WLAN, nur Kurzwerte)</label>
<div id="bleBox" style="margin-left:28px"><label class="f"><span>Bluetooth-Passwort (wie im Panel unter Einstellungen → Geräte → Satelliten)</span><input id="bPw" maxlength="32" autocomplete="off"></label>
<p class="hint" id="bleName"></p></div>

<h2 style="margin-top:14px">Bluetooth-Geräte auswerten</h2>
<label class="ck"><input type="checkbox" id="pTruma">Truma LevelControl</label>
<label class="ck"><input type="checkbox" id="pMopeka">Mopeka</label>
<label class="ck"><input type="checkbox" id="pVictron">Victron (Instant Readout)</label>
<label class="ck" id="pHymerL" style="display:none"><input type="checkbox" id="pHymer">Hymer Connect (SCU im Fahrzeug)</label>
<div class="row" style="margin-top:10px"><button class="p" id="bSave" style="flex:none">Speichern</button></div>
</div>

<div class="card"><h2>Anschlüsse (GPIO)</h2>
<p class="hint">Hier legst du fest, was an den Pins des Satelliten hängt. Die Werte gehen auf denselben Wegen ans Panel wie die Bluetooth-Geräte (MQTT mit Home Assistant, UDP, Bluetooth).</p>
<div class="row"><label class="f"><span>Sensoren abfragen alle … Sekunden</span><input id="ioInt" type="number" min="2" max="3600"></label>
<label class="f"><span>Tanksonden abfragen alle … Sekunden</span><input id="ioTInt" type="number" min="2" max="65535"></label></div>
<div id="ioBody"><p class="hint">Lädt …</p></div>
<div class="row" style="margin-top:10px"><button class="p" id="bIoSave" style="flex:none">Belegung speichern</button><button id="bIoTank" style="flex:none">Tanks jetzt messen</button></div>
</div>

<div class="card"><h2>Truma LevelControl</h2><div id="trList"></div>
<div class="row" style="margin-top:8px"><label class="f" style="flex:none;width:200px"><span>Messen alle … Minuten</span><input id="trInt" type="number" min="1" max="1440"></label></div>
<div class="row"><button id="bTrScan" style="flex:none">Sensoren suchen</button><button id="bTrMeasure" style="flex:none">Jetzt messen</button></div>
<div id="trFound"></div></div>

<div class="card"><h2>Mopeka</h2><div id="moList"></div>
<div class="row" style="margin-top:8px"><button id="bMoScan" style="flex:none">Sensoren suchen</button></div>
<div id="moFound"></div></div>

<div class="card"><h2>Victron</h2>
<p class="hint">Pro Gerät den Schlüssel aus VictronConnect: Einstellungen → Produktinfo → Instant Readout → Details anzeigen.</p>
<div id="viList"></div>
<div class="row" style="margin-top:8px"><button id="bViScan" style="flex:none">Geräte suchen</button></div>
<div id="viFound"></div></div>

<div class="card" id="hyCard" style="display:none"><h2>Hymer Connect</h2>
<p class="hint">Liest Licht, Batterie, Tanks und Schalter direkt von der SCU im Hymer (bzw. Bürstner, Dethleffs, Eriba, LMC …) – ohne Cloud. Koppeln: hier „Koppeln“ drücken, dann am Bedienteil im Fahrzeug „CONNECTION“ drücken (3 Minuten Zeit). Die Werte gehen wie die anderen Anschlüsse ans Panel.</p>
<p class="hint" id="hyStatus"></p>
<div id="hyList"></div>
<div class="row" style="margin-top:8px"><button id="bHyPair" style="flex:none">Koppeln</button><button class="d" id="bHyForget" style="flex:none">Kopplung lösen</button></div></div>

<div class="card"><h2>Firmware</h2>
<p class="hint" id="fwInfo"></p>
<div class="row"><input type="file" id="fwFile" accept=".bin"><button id="bFw" style="flex:none">Hochladen</button><button id="bReboot" style="flex:none">Neu starten</button></div>
</div>
</main><div id="toast"></div>
<script>
const $=s=>document.querySelector(s);let S=null,loaded=false;
const esc=s=>String(s==null?'':s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
function toast(t,err){const e=$('#toast');e.textContent=t;e.style.display='block';e.style.color=err?'var(--err)':'';clearTimeout(e._t);e._t=setTimeout(()=>e.style.display='none',3500)}
async function post(u,d){const r=await fetch(u,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(d)});const j=await r.json().catch(()=>({}));if(!r.ok)throw new Error(j.error||r.statusText);return j}
const act=(u,d,msg)=>post(u,d).then(()=>{if(msg)toast(msg);load()}).catch(e=>toast(e.message,true));
const age=s=>s==null||s<0?'noch nichts':s<60?'gerade eben':s<3600?'vor '+Math.round(s/60)+' Min.':'vor '+Math.round(s/3600)+' Std.';
const sig=r=>r>-65?'stark':r>-80?'mittel':'schwach';
const num=(v,d,u)=>v==null?'–':v.toFixed(d).replace('.',',')+' '+u;
function fill(){
 $('#cName').value=S.name;$('#cId').value=S.id;$('#cSsid').value=S.wifi.ssid;
 $('#mMqtt').checked=S.modes.mqtt;$('#mUdp').checked=S.modes.udp;$('#mBle').checked=S.modes.ble;
 $('#qHost').value=S.mqtt.host;$('#qPort').value=S.mqtt.port;$('#qUser').value=S.mqtt.user;$('#qBase').value=S.mqtt.base;
 $('#qHa').value=S.mqtt.ha_prefix;$('#qHaOn').checked=S.mqtt.ha;$('#qPass').placeholder=S.mqtt.has_pass?'unverändert':'kein Passwort';
 $('#uIp').value=S.udp_target;$('#bPw').value=S.ble_pw;$('#pTruma').checked=S.ble.truma;$('#pMopeka').checked=S.ble.mopeka;$('#pVictron').checked=S.ble.victron;if('hymer' in S.ble){$('#pHymerL').style.display='';$('#pHymer').checked=S.ble.hymer}
 $('#trInt').value=S.truma.interval;boxes();
}
function boxes(){$('#mqttBox').style.display=$('#mMqtt').checked?'':'none';$('#udpBox').style.display=$('#mUdp').checked?'':'none';$('#bleBox').style.display=$('#mBle').checked?'':'none'}
function render(){
 $('#hName').textContent=S.name;document.title=S.name+' – PanelSat';
 $('#hSub').textContent='Kennung '+S.id+(S.wifi.ip?' · '+S.wifi.ip:'');
 const c=[];c.push(S.wifi.connected?['ok','WLAN '+S.wifi.ssid+' ('+S.wifi.rssi+' dBm)']:['err',S.wifi.ssid?'WLAN getrennt':'kein WLAN eingerichtet']);
 if(S.wifi.ap)c.push(['','Einrichtungs-WLAN '+S.wifi.ap_ssid]);
 if(S.modes.mqtt)c.push([S.mqtt.connected?'ok':'err','MQTT: '+S.mqtt.status]);
 if(S.modes.udp)c.push(['ok','UDP: '+S.udp_sent+' Pakete']);
 if(S.modes.ble)c.push(['ok','Bluetooth: '+S.adv_count+' Gerät(e)']);
 $('#chips').innerHTML=c.map(x=>`<span class="chip ${x[0]}">${esc(x[1])}</span>`).join('');
 $('#bleName').textContent='Per Bluetooth heißt dieser Satellit „'+S.ble_name+'“. Das Panel nimmt ihn automatisch auf, wenn das Passwort passt.';
 $('#fwInfo').textContent='Version '+S.fw+(S.chip?' ('+S.chip+')':'')+', freier Speicher '+Math.round(S.heap/1024)+' KB, läuft seit '+Math.round(S.uptime/60)+' Min.';
 if(document.activeElement&&document.activeElement.closest('.card')&&document.activeElement.closest('#trList,#moList,#viList'))return;
 const T=S.truma;
 $('#trList').innerHTML=T.sensors.length?T.sensors.map(x=>`<div class="dev"><div class="t"><input value="${esc(x.name)}" data-trn="${x.mac}" maxlength="32">
  <div class="v">${x.has?x.percent+' % · '+num(x.kg,1,'kg')+' · Akku '+x.battery+' % · '+age(x.age):esc(x.status)}${x.busy?' · misst …':''} · <span class="mono">${x.mac}</span></div></div>
  <button data-trm="${x.mac}">Messen</button><button class="d" data-trr="${x.mac}">Entfernen</button></div>`).join(''):'<p class="hint">Noch kein Sensor eingerichtet.</p>';
 $('#bTrScan').disabled=T.scanning||T.sensors.length>=T.max;$('#bTrScan').textContent=T.scanning?'Suche läuft …':'Sensoren suchen';
 $('#trFound').innerHTML=T.scanning||T.found.length?'<h2 style="margin-top:12px">Gefunden</h2>'+(T.found.length?T.found.map((f,i)=>`<div class="dev"><div class="t mono">${f.mac}<div class="v">Signal ${sig(f.rssi)}${f.configured?', eingerichtet':''}</div></div><button data-tra="${i}" ${f.configured?'disabled':''}>Hinzufügen</button></div>`).join(''):'<p class="hint">Noch nichts gefunden …</p>'):'';
 const M=S.mopeka;
 $('#moList').innerHTML=M.sensors.length?M.sensors.map(x=>`<div class="dev"><div class="t"><input value="${esc(x.name)}" data-mon="${x.mac}" maxlength="32">
  <div class="v">${esc(x.model)} · ${x.has?x.percent+' % · '+x.mm+' mm'+(x.capacity>0?' · '+num(x.amount,1,x.unit):''):esc(x.status)} · Akku ${x.battery} % · <span class="mono">${x.mac}</span></div></div>
  <select data-mot="${x.mac}">${M.tanks.map((t,i)=>`<option value="${i}" ${i===x.tank?'selected':''}>${esc(t)}</option>`).join('')}${x.tank===255?'<option selected value="255">eigene Werte</option>':''}</select>
  <button class="d" data-mor="${x.mac}">Entfernen</button></div>`).join(''):'<p class="hint">Noch kein Sensor eingerichtet.</p>';
 $('#bMoScan').disabled=M.scanning||M.sensors.length>=M.max;$('#bMoScan').textContent=M.scanning?'Suche läuft … (Taste am Sensor drücken)':'Sensoren suchen';
 $('#moFound').innerHTML=M.scanning||M.found.length?'<h2 style="margin-top:12px">Gefunden</h2>'+(M.found.length?M.found.map((f,i)=>`<div class="dev"><div class="t">${esc(f.model)} <span class="mono">${f.mac}</span><div class="v">Signal ${sig(f.rssi)}${f.button?', Taste gedrückt':''}${f.configured?', eingerichtet':''}</div></div><button data-moa="${i}" ${f.configured?'disabled':''}>Hinzufügen</button></div>`).join(''):'<p class="hint">Noch nichts gefunden …</p>'):'';
 const H=S.hymer;$('#hyCard').style.display=H&&S.ble.hymer?'':'none';
 if(H){$('#hyStatus').textContent=(H.connected?'Verbunden':H.paired?'Gekoppelt':'Nicht gekoppelt')+' – '+H.status;
  $('#hyList').innerHTML=H.values.length?H.values.map(x=>`<div class="dev"><div class="t">${esc(x.name)}<div class="v">${x.on!=null?(x.on?(x.switch||x.onoff?'An':'zu'):(x.switch||x.onoff?'Aus':'offen')):x.value!=null?num(x.value,x.unit==='%'?0:2,x.unit):'–'}</div></div>${x.switch?`<button data-hys="${x.id}" data-on="${x.on?0:1}">${x.on?'Aus':'An'}</button>`:''}</div>`).join(''):'<p class="hint">Noch keine Werte.</p>';
  $('#bHyPair').disabled=H.pairing;$('#bHyPair').textContent=H.pairing?'Kopplung läuft … (CONNECTION drücken)':H.paired?'Neu koppeln':'Koppeln';$('#bHyForget').style.display=H.paired?'':'none'}
 const V=S.victron;
 $('#viList').innerHTML=V.devices.length?V.devices.map(x=>`<div class="dev"><div class="t"><input value="${esc(x.name)}" data-vin="${x.mac}" maxlength="32">
  <div class="v">${esc(x.kind_text)} · ${x.has?[x.pv!=null?num(x.pv,0,'W'):'',x.soc!=null?num(x.soc,0,'%'):'',num(x.v,2,'V'),num(x.i,1,'A'),x.state_text||''].filter(Boolean).join(' · '):esc(x.status)} · <span class="mono">${x.mac}</span></div></div>
  <button data-vik="${x.mac}">Schlüssel</button><button class="d" data-vir="${x.mac}">Entfernen</button></div>`).join(''):'<p class="hint">Noch kein Gerät eingerichtet.</p>';
 $('#bViScan').disabled=V.scanning||V.devices.length>=V.max;$('#bViScan').textContent=V.scanning?'Suche läuft …':'Geräte suchen';
 $('#viFound').innerHTML=V.scanning||V.found.length?'<h2 style="margin-top:12px">Gefunden</h2>'+(V.found.length?V.found.map((f,i)=>`<div class="dev"><div class="t">${esc(f.name||'Victron')} <span class="mono">${f.mac}</span><div class="v">Signal ${sig(f.rssi)}${f.configured?', eingerichtet':''}</div></div><button data-via="${i}" ${f.configured?'disabled':''}>Hinzufügen</button></div>`).join(''):'<p class="hint">Noch nichts gefunden …</p>'):'';
}
async function load(){try{S=await (await fetch('/api/state')).json();if(!loaded){fill();loaded=true}render();ioLive()}catch(e){$('#hSub').textContent='Keine Verbindung zum Satelliten …'}}
// ---------------- Anschluesse (GPIO)
let IO=null,PINS=[];
const CLS=[['','Allgemein'],['temperature','Temperatur'],['humidity','Luftfeuchte'],['level','Füllstand'],['voltage','Spannung'],['pressure','Druck'],['current','Strom']];
const P5=[0,1,2,3,4];
function ioUsed(){const u=new Set(),C=IO;C.tanks.forEach(t=>{if(t.mode==1){t.pins.forEach(p=>p>=0&&u.add(p));t.gnd>=0&&u.add(t.gnd)}if(t.mode==2&&t.apin>=0)u.add(t.apin)});if(C.ow_pin>=0)u.add(C.ow_pin);C.analog.forEach(a=>a.pin>=0&&u.add(a.pin));if(C.mpu.on){u.add(C.mpu.sda);u.add(C.mpu.scl)}C.switches.forEach(x=>x.pin>=0&&u.add(x.pin));C.inputs.forEach(x=>x.pin>=0&&u.add(x.pin));return u}
const fits=(p,need)=>(need!='out'||p.out)&&(need!='adc'||p.adc);
function freePin(need){const u=ioUsed();const f=PINS.filter(p=>!u.has(p.pin)&&fits(p,need));return (f.find(p=>!p.note)||f[0]||{pin:-1}).pin}
function pinSel(k,v,need,opt){const u=ioUsed();return `<select data-k="${k}" data-n data-r>${opt?`<option value="-1" ${v<0?'selected':''}>– keiner –</option>`:''}${PINS.filter(p=>fits(p,need)).map(p=>`<option value="${p.pin}" ${p.pin==v?'selected':''}>GPIO ${p.pin}${u.has(p.pin)&&p.pin!=v?' (belegt)':''}${p.note?' – '+esc(p.note):''}</option>`).join('')}</select>`}
const fld=(l,h,st)=>`<label class="f"${st?` style="${st}"`:''}><span>${l}</span>${h}</label>`;
const inp=(k,v,a)=>`<input data-k="${k}" value="${esc(v)}" ${a||''}>`;
const nin=(k,v,a)=>inp(k,v,'type="number" data-n '+(a||''));
const ck=(k,v,l)=>`<label class="ck"><input type="checkbox" data-k="${k}" ${v?'checked':''}>${l}</label>`;
const sel=(k,v,opts,r)=>`<select data-k="${k}" ${r?'data-r':''} ${typeof opts[0][0]=='number'?'data-n':''}>${opts.map(o=>`<option value="${o[0]}" ${o[0]==v?'selected':''}>${o[1]}</option>`).join('')}</select>`;
function setK(k,v){const ps=k.split('.');let o=IO;for(let i=0;i<ps.length-1;i++)o=o[ps[i]];o[ps[ps.length-1]]=v}
function ioRender(){const C=IO;$('#ioInt').value=C.interval;$('#ioTInt').value=C.tank_interval;let h='';
 // Tanks
 h+='<h2 class="sec">Tanks mit Sonden</h2><p class="hint">Bis zu 5 Sonden (Stangen verschiedener Länge) je Tank. Jede Sonde bekommt ihre Prozentstufe; angezeigt wird die höchste Stufe mit Wasserkontakt. Die Sonden werden nur zum Messen kurz bestromt.</p>';
 C.tanks.forEach((t,i)=>{if(!t.mode)return;const b=`tanks.${i}`;
  h+=`<div class="io"><div class="ioh"><b>Tank ${i+1}</b><span class="live" id="lt${i}"></span><button class="d s" data-iodel="tank:${i}">Entfernen</button></div>
  <div class="row">${fld('Name',inp(b+'.name',t.name,'maxlength="32" placeholder="z. B. Frischwasser"'))}${fld('Anschluss',sel(b+'.mode',t.mode,[[1,'je Sonde ein GPIO'],[2,'Widerstandsleiter an einem Analog-Pin']],1))}</div>`;
  if(t.mode==1){
   h+=`<div class="row">${fld('Masse-Stange an GPIO (optional)',pinSel(b+'.gnd',t.gnd,'out',1))}${fld('Kontakt bedeutet',sel(b+'.inv',t.inv?1:0,[[0,'Low – Sonde gegen Masse'],[1,'High – Modul mit Ausgang']]))}</div>`;
   h+=P5.map(p=>`<div class="pr"><span class="lbl">Sonde ${p+1}</span>${fld('GPIO',pinSel(`${b}.pins.${p}`,t.pins[p],'in',1))}${fld('Stufe in %',nin(`${b}.pct.${p}`,t.pct[p],'min="0" max="100"'),'flex:.5')}</div>`).join('');
   {const np=PINS.filter(p=>!p.pull).map(p=>p.pin);h+='<p class="hint">Gemeinsame Stange an GND (oder an den Masse-GPIO).'+(np.length?` GPIO ${np.join(', ')} brauchen einen externen Pull-up (z. B. 47 kΩ an 3,3 V).`:'')+'</p>'}
  }else{
   h+=`<div class="row">${fld('Analog-Pin',pinSel(b+'.apin',t.apin,'adc',1))}${fld('Spannung ohne Kontakt (mV)',nin(b+'.mv_empty',t.mv_empty,'min="0" max="3300"'))}<button class="s" style="flex:none;align-self:center" data-cap="${b}.mv_empty|tank:${i}">übernehmen</button></div>`;
   h+=P5.map(p=>`<div class="pr"><span class="lbl">Sonde ${p+1}</span>${fld('Stufe in %',nin(`${b}.pct.${p}`,t.pct[p],'min="0" max="100"'),'flex:.5')}${fld('Spannung (mV)',nin(`${b}.mv.${p}`,t.mv[p],'min="0" max="3300"'))}<button class="s" style="flex:none;margin-bottom:10px" data-cap="${b}.mv.${p}|tank:${i}">übernehmen</button></div>`).join('');
   h+='<p class="hint">Zum Einlernen: Wasser bis zur Sonde, „Tanks jetzt messen“, dann „übernehmen“. Sonden mit 0 mV zählen nicht.</p>';
  }
  h+='</div>'});
 if(C.tanks.some(t=>!t.mode))h+='<button data-ioadd="tank">+ Tank</button>';
 // DS18B20
 h+=`<h2 class="sec">Temperatur (DS18B20)</h2><div class="row">${fld('1-Wire-Bus an GPIO',pinSel('ow_pin',C.ow_pin,'out',1))}<button style="flex:none;align-self:center" data-ioact='{"ds_search":true}'>Fühler suchen</button></div>
  <p class="hint">Mehrere Fühler an einem Draht: DQ an den GPIO, 4,7 kΩ von DQ nach 3,3 V, VDD an 3,3 V. Erst den GPIO speichern, dann suchen.</p>`;
 C.ds.forEach((d,i)=>{if(!d.addr)return;h+=`<div class="io"><div class="ioh"><b>Temperatur ${i+1}</b><span class="live" id="ld${i}"></span><button class="d s" data-iodel="ds:${i}">Entfernen</button></div>
  <div class="row">${fld('Name',inp(`ds.${i}.name`,d.name,'maxlength="32" placeholder="z. B. Innen"'))}${fld('Korrektur (°C)',nin(`ds.${i}.off`,d.off,'step="0.1" min="-10" max="10"'),'flex:.5')}</div><div class="v mono live">${d.addr}</div></div>`});
 // Analog
 h+='<h2 class="sec">Analogsensoren</h2><p class="hint">Nur GPIO '+PINS.filter(p=>p.adc).map(p=>p.pin).join(', ')+' (die anderen Analogeingänge sind bei WLAN nicht nutzbar), 0–3,1 V. Umrechnung über zwei Punkte.</p>';
 C.analog.forEach((a,i)=>{if(a.pin<0)return;const b=`analog.${i}`;
  h+=`<div class="io"><div class="ioh"><b>Analog ${i+1}</b><span class="live" id="la${i}"></span><button class="d s" data-iodel="analog:${i}">Entfernen</button></div>
  <div class="row">${fld('Name',inp(b+'.name',a.name,'maxlength="32"'))}${fld('GPIO',pinSel(b+'.pin',a.pin,'adc'))}</div>
  <div class="row">${fld('Art',sel(b+'.class',a.class,CLS))}${fld('Einheit',inp(b+'.unit',a.unit,'maxlength="8" placeholder="Standard der Art"'),'flex:.5')}${fld('Nachkommastellen',nin(b+'.dec',a.dec,'min="0" max="3"'),'flex:.5')}</div>
  <div class="pr"><span class="lbl">Punkt 1</span>${fld('mV',nin(b+'.mv0',a.mv0,'min="0" max="3300"'))}${fld('Wert',nin(b+'.v0',a.v0,'step="any"'))}<button class="s" style="flex:none;margin-bottom:10px" data-cap="${b}.mv0|an:${i}">übernehmen</button></div>
  <div class="pr"><span class="lbl">Punkt 2</span>${fld('mV',nin(b+'.mv1',a.mv1,'min="0" max="3300"'))}${fld('Wert',nin(b+'.v1',a.v1,'step="any"'))}<button class="s" style="flex:none;margin-bottom:10px" data-cap="${b}.mv1|an:${i}">übernehmen</button></div></div>`});
 if(C.analog.some(a=>a.pin<0))h+='<button data-ioadd="analog">+ Analogsensor</button>';
 // MPU
 const m=C.mpu;h+=`<h2 class="sec">Neigung (GY-521 / MPU-6050)</h2>${ck('mpu.on',m.on,'MPU-6050 angeschlossen')}`;
 if(m.on)h+=`<div class="io"><div class="ioh"><b>Neigung</b><span class="live" id="lm"></span></div>
  <div class="row">${fld('SDA',pinSel('mpu.sda',m.sda,'out'))}${fld('SCL',pinSel('mpu.scl',m.scl,'out'))}${fld('Adresse',sel('mpu.addr',m.addr,[[104,'0x68 (AD0 an GND)'],[105,'0x69 (AD0 an 3,3 V)']]))}</div>
  <div class="row">${fld('Glättung (Sekunden)',nin('mpu.smooth',m.smooth,'step="0.5" min="0" max="60"'))}${fld('Totband (Grad)',nin('mpu.dead',m.dead,'step="0.1" min="0" max="5"'))}</div>
  ${ck('mpu.still',m.still,'nur im Stillstand messen (bei Bewegung den letzten Wert halten)')}
  ${m.still?`<div class="row">${fld('Bewegung ab … Grad/s (Gyro)',nin('mpu.still_dps',m.still_dps,'step="0.5" min="0.5" max="50"'),'flex:.5')}</div>`:''}
  <p class="hint">Gegen Wackeln: Ausreißer werden verworfen und die Werte über die Glättungszeit gemittelt. Das Totband meldet erst eine Änderung ab dieser Größe. Läuft jemand im Fahrzeug, bleibt der letzte ruhige Wert stehen.</p>
  ${ck('mpu.swap',m.swap,'Achsen tauschen (längs ↔ quer)')}${ck('mpu.inv_x',m.inv_x,'längs umkehren')}${ck('mpu.inv_y',m.inv_y,'quer umkehren')}
  <div class="row" style="margin-top:6px"><button style="flex:none" data-ioact='{"tilt_zero":true}'>Nullpunkt setzen (Fahrzeug steht gerade)</button></div></div>`;
 // Schalter
 h+='<h2 class="sec">Schalter / Relais</h2>';
 C.switches.forEach((x,i)=>{if(x.pin<0)return;const b=`switches.${i}`;
  h+=`<div class="io"><div class="ioh"><b>Schalter ${i+1}</b><span class="live" id="ls${i}"></span><button class="s" data-iosw="${i+1}">Umschalten</button><button class="d s" data-iodel="switch:${i}">Entfernen</button></div>
  <div class="row">${fld('Name',inp(b+'.name',x.name,'maxlength="32" placeholder="z. B. Wasserpumpe"'))}${fld('GPIO',pinSel(b+'.pin',x.pin,'out'))}</div>
  <div class="row">${fld('Ansteuerung',sel(b+'.low',x.low?1:0,[[0,'High = an'],[1,'Low = an (viele Relaismodule)']]))}${fld('Nach dem Start',sel(b+'.boot',x.boot,[[0,'aus'],[1,'an'],[2,'wie zuletzt']]))}</div></div>`});
 if(C.switches.some(x=>x.pin<0))h+='<button data-ioadd="switch">+ Schalter</button>';
 // Eingaenge
 h+='<h2 class="sec">Eingänge (Kontakt, Taster)</h2>';
 C.inputs.forEach((x,i)=>{if(x.pin<0)return;const b=`inputs.${i}`;
  h+=`<div class="io"><div class="ioh"><b>Eingang ${i+1}</b><span class="live" id="li${i}"></span><button class="d s" data-iodel="input:${i}">Entfernen</button></div>
  <div class="row">${fld('Name',inp(b+'.name',x.name,'maxlength="32" placeholder="z. B. Tür"'))}${fld('GPIO',pinSel(b+'.pin',x.pin,'in'))}</div>
  ${ck(b+'.pull',x.pull,'interner Pull-up (Kontakt schaltet gegen Masse)')}${ck(b+'.inv',x.inv,'umkehren')}</div>`});
 if(C.inputs.some(x=>x.pin<0))h+='<button data-ioadd="input">+ Eingang</button>';
 $('#ioBody').innerHTML=h;ioLive()}
const fmt=(v,d)=>v==null?'–':(+v).toFixed(d).replace('.',',');
function ioLive(){if(!IO||!S||!S.io)return;const L=S.io;
 IO.tanks.forEach((t,i)=>{const e=$('#lt'+i);if(!e)return;const x=L.tanks[i];const used=P5.filter(p=>t.mode==1?t.pins[p]>=0:(t.pct[p]&&t.mv[p]));
  e.innerHTML=x.ok?`<b style="color:var(--tx)">${x.pct} %</b> `+used.map(p=>`<span title="Sonde ${p+1}" style="color:${x.probes>>p&1?'var(--ac)':'var(--tx2)'}">${x.probes>>p&1?'●':'○'}</span>`).join('')+(t.mode==2?` · ${x.mv} mV`:'')+(x.odd?' · <span style="color:var(--warn)">unplausibel</span>':''):(t.mode==2&&x.mv>=0?x.mv+' mV':'noch keine Messung')});
 IO.ds.forEach((d,i)=>{const e=$('#ld'+i);if(e)e.textContent=L.ds[i].ok?fmt(L.ds[i].t,1)+' °C':'keine Antwort'});
 IO.analog.forEach((a,i)=>{const e=$('#la'+i);if(e)e.textContent=L.analog[i].mv<0?'noch keine Messung':L.analog[i].mv+' mV → '+fmt(L.analog[i].v,a.dec)+' '+(a.unit||'')});
 const m=$('#lm');if(m)m.innerHTML=L.mpu.ok?(L.mpu.x!=null?`längs ${fmt(L.mpu.x,1)}° · quer ${fmt(L.mpu.y,1)}°`+(L.mpu.moving?' · <span style="color:var(--warn)">bewegt sich – Wert gehalten</span>':''):'misst …'):`<span style="color:var(--err)">${esc(L.mpu.error||'nicht gespeichert')}</span>`;
 IO.switches.forEach((x,i)=>{const e=$('#ls'+i);if(e)e.innerHTML=L.switches[i]?'<b style="color:var(--ok)">an</b>':'aus'});
 IO.inputs.forEach((x,i)=>{const e=$('#li'+i);if(e)e.textContent=L.inputs[i]?'Kontakt geschlossen':'offen'});
 const io=S.io;void io}
async function ioLoad(){try{const r=await (await fetch('/api/io')).json();IO=r.config;PINS=r.pins;ioRender()}catch(e){$('#ioBody').innerHTML='<p class="hint">Belegung konnte nicht geladen werden.</p>'}}
document.addEventListener('change',e=>{const t=e.target;if(!t.dataset||!t.dataset.k||!IO)return;let v=t.type=='checkbox'?t.checked:('n' in t.dataset)?+t.value:t.value;
 if(t.dataset.k.endsWith('.inv')&&t.tagName=='SELECT'||t.dataset.k.endsWith('.low'))v=!!+t.value;setK(t.dataset.k,v);if('r' in t.dataset||t.type=='checkbox'&&(t.dataset.k=='mpu.on'||t.dataset.k=='mpu.still'))ioRender()});
$('#ioInt').onchange=e=>IO&&(IO.interval=+e.target.value);$('#ioTInt').onchange=e=>IO&&(IO.tank_interval=+e.target.value);
$('#bIoSave').onclick=()=>post('/api/io',IO).then(()=>{toast('Belegung gespeichert.');ioLoad();setTimeout(load,800)}).catch(e=>toast(e.message,true));
$('#bIoTank').onclick=()=>act('/api/io/action',{tanks_now:true},'Tanks werden gemessen.');
document.addEventListener('click',e=>{const b=e.target.closest('button');if(!b||!IO)return;const d=b.dataset;
 if(d.ioadd){const C=IO;
  if(d.ioadd=='tank'){const t=C.tanks.find(t=>!t.mode);t.mode=1;t.name=''}
  if(d.ioadd=='analog'){const a=C.analog.find(a=>a.pin<0);a.pin=freePin('adc');if(a.pin<0)return toast('Kein Analog-Pin mehr frei',true)}
  if(d.ioadd=='switch'){const x=C.switches.find(x=>x.pin<0);x.pin=freePin('out');if(x.pin<0)return toast('Kein GPIO mehr frei',true)}
  if(d.ioadd=='input'){const x=C.inputs.find(x=>x.pin<0);x.pin=freePin('in');if(x.pin<0)return toast('Kein GPIO mehr frei',true)}
  ioRender();toast('Hinzugefügt – Pin prüfen und „Belegung speichern“.')}
 if(d.iodel){const [k,i]=d.iodel.split(':');const C=IO;
  if(k=='tank'){C.tanks[i].mode=0;C.tanks[i].pins=[-1,-1,-1,-1,-1];C.tanks[i].gnd=-1;C.tanks[i].apin=-1}
  if(k=='ds'){C.ds[i].addr='';C.ds[i].name=''}
  if(k=='analog')C.analog[i].pin=-1;if(k=='switch')C.switches[i].pin=-1;if(k=='input')C.inputs[i].pin=-1;
  ioRender();toast('Entfernt – mit „Belegung speichern“ übernehmen.')}
 if(d.cap){const [k,src]=d.cap.split('|');const [w,i]=src.split(':');const mv=w=='tank'?S.io.tanks[i].mv:S.io.analog[i].mv;
  if(mv==null||mv<0)return toast('Noch kein Messwert – erst speichern und messen lassen.',true);setK(k,mv);ioRender();toast(mv+' mV übernommen.')}
 if(d.ioact)act('/api/io/action',JSON.parse(d.ioact),'Erledigt.').then(()=>ioLoad());
 if(d.iosw)act('/api/io/action',{switch:+d.iosw});
});
$('#mMqtt').onchange=$('#mUdp').onchange=$('#mBle').onchange=boxes;
$('#bSave').onclick=()=>{
 const d={name:$('#cName').value.trim(),id:$('#cId').value.trim(),wifi:{ssid:$('#cSsid').value.trim()},
  mqtt:{host:$('#qHost').value.trim(),port:+$('#qPort').value||1883,user:$('#qUser').value.trim(),base:$('#qBase').value.trim(),ha:$('#qHaOn').checked,ha_prefix:$('#qHa').value.trim()},
  modes:{mqtt:$('#mMqtt').checked,udp:$('#mUdp').checked,ble:$('#mBle').checked},udp_target:$('#uIp').value.trim(),ble_pw:$('#bPw').value.trim(),
  ble:{truma:$('#pTruma').checked,mopeka:$('#pMopeka').checked,victron:$('#pVictron').checked,...('hymer' in S.ble?{hymer:$('#pHymer').checked}:{})}};
 if($('#cWpass').value)d.wifi.pass=$('#cWpass').value;if($('#qPass').value)d.mqtt.pass=$('#qPass').value;
 post('/api/config',d).then(r=>{toast(r.restart?'Gespeichert – der Satellit startet neu und verbindet sich mit dem WLAN.':'Gespeichert.');$('#cWpass').value='';$('#qPass').value='';loaded=false;setTimeout(load,r.restart?8000:500)}).catch(e=>toast(e.message,true));
};
$('#bScanW').onclick=async()=>{$('#bScanW').disabled=true;$('#bScanW').textContent='Suche …';try{const r=await (await fetch('/api/wifiscan')).json();$('#nets').innerHTML=r.networks.map(n=>`<option value="${esc(n.ssid)}">${n.rssi} dBm</option>`).join('');toast(r.networks.length+' Netze gefunden – ins Feld tippen zum Auswählen.')}catch(e){toast('Suche fehlgeschlagen',true)}$('#bScanW').disabled=false;$('#bScanW').textContent='Netze suchen'};
$('#trInt').onchange=e=>act('/api/truma',{interval:+e.target.value},'Gespeichert.');
$('#bTrScan').onclick=()=>act('/api/truma',{scan:true},'Suche gestartet (bis zu 3 Minuten).');
$('#bTrMeasure').onclick=()=>act('/api/truma',{measure:''},'Messung gestartet.');
$('#bMoScan').onclick=()=>act('/api/mopeka',{scan:true},'Suche gestartet – Taste am Sensor drücken.');
$('#bHyPair').onclick=()=>act('/api/hymer',{pair:true},'Jetzt am Bedienteil im Fahrzeug „CONNECTION“ drücken.');
$('#bHyForget').onclick=()=>{if(confirm('Kopplung mit der SCU lösen?'))act('/api/hymer',{forget:true})};
$('#bViScan').onclick=()=>act('/api/victron',{scan:true},'Suche gestartet.');
document.addEventListener('change',e=>{const t=e.target,d=t.dataset;
 if(d.trn)act('/api/truma',{rename:{mac:d.trn,name:t.value}},'Name gespeichert.');
 if(d.mon)act('/api/mopeka',{rename:{mac:d.mon,name:t.value}},'Name gespeichert.');
 if(d.vin)act('/api/victron',{rename:{mac:d.vin,name:t.value}},'Name gespeichert.');
 if(d.mot){const x=S.mopeka.sensors.find(s=>s.mac===d.mot);act('/api/mopeka',{tank:{mac:d.mot,tank:+t.value,empty:x.empty,full:x.full,capacity:x.capacity}},'Gespeichert.')}
});
document.addEventListener('click',e=>{const b=e.target.closest('button');if(!b)return;const d=b.dataset;
 if(d.trm)act('/api/truma',{measure:d.trm},'Messung gestartet.');
 if(d.trr&&confirm('Sensor entfernen?'))act('/api/truma',{remove:d.trr});
 if(d.tra){const f=S.truma.found[+d.tra];const n=prompt('Name, z. B. „Flasche links“','Gasflasche '+(S.truma.sensors.length+1));if(n!=null)act('/api/truma',{add:{mac:f.mac,addr_type:f.addr_type,name:n}},'Eingerichtet.')}
 if(d.mor&&confirm('Sensor entfernen?'))act('/api/mopeka',{remove:d.mor});
 if(d.moa){const f=S.mopeka.found[+d.moa];const n=prompt('Name, z. B. „Flasche links“','Gasflasche '+(S.mopeka.sensors.length+1));if(n!=null)act('/api/mopeka',{add:{mac:f.mac,name:n,tank:0}},'Eingerichtet – Flaschentyp wählen.')}
 if(d.hys)act('/api/hymer',{switch:d.hys,on:d.on==='1'});
 if(d.vir&&confirm('Gerät entfernen?'))act('/api/victron',{remove:d.vir});
 if(d.vik){const k=prompt('Schlüssel (32 Zeichen)');if(k)act('/api/victron',{key:{mac:d.vik,key:k.replace(/[\s:-]/g,'')}},'Schlüssel gespeichert.')}
 if(d.via){const f=S.victron.found[+d.via];const k=prompt('Schlüssel (32 Zeichen) für '+(f.name||f.mac));if(k){const n=prompt('Name',f.name||'Victron');act('/api/victron',{add:{mac:f.mac,key:k.replace(/[\s:-]/g,''),name:n||''}},'Eingerichtet.')}}
});
$('#bFw').onclick=async()=>{const f=$('#fwFile').files[0];if(!f)return toast('Erst eine .bin-Datei wählen',true);const fd=new FormData();fd.append('fw',f);$('#bFw').disabled=true;$('#bFw').textContent='Lädt hoch …';
 try{const r=await fetch('/api/update',{method:'POST',body:fd});const j=await r.json();if(j.ok){toast('Firmware übernommen – Neustart …');setTimeout(()=>location.reload(),9000)}else toast(j.error||'Fehlgeschlagen',true)}catch(e){toast('Hochladen fehlgeschlagen',true)}$('#bFw').disabled=false;$('#bFw').textContent='Hochladen'};
$('#bReboot').onclick=()=>confirm('Satellit neu starten?')&&act('/api/reboot',{},'Startet neu …');
load();ioLoad();setInterval(load,3000);
</script></body></html>)HTML";
