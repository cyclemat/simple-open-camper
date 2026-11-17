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
.dpp{margin:0 0 14px}.dph{display:flex;align-items:center;gap:8px;margin:0 0 6px}.dph b{flex:1}.dph select{width:auto;padding:4px 8px}
.scr{width:100%;max-width:320px;aspect-ratio:4/3;background:#000;border-radius:10px;padding:5px;display:grid;gap:5px}
.scr.n1{grid-template:1fr/1fr}.scr.n2{grid-template:1fr 1fr/1fr}.scr.n4{grid-template:1fr 1fr/1fr 1fr}
.tl{background:#1E252C;border:1px solid var(--line);border-radius:9px;padding:6px 9px;overflow:hidden;cursor:pointer;display:flex;flex-direction:column;justify-content:space-between;min-width:0;user-select:none;-webkit-user-select:none}
.tl .tn{font-size:12px;color:var(--tx2);white-space:nowrap;overflow:hidden;text-overflow:ellipsis}.tl .tv{font-size:15px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.scr.n1 .tv{font-size:30px}.scr.n2 .tv{font-size:21px}.scr.n1 .tn{font-size:14px}
.tl.sel{border-color:var(--ac);box-shadow:0 0 0 2px var(--ac)}.tl.em{background:transparent;border-style:dashed}.tl.ov{border-color:var(--ok)}
</style></head><body><main>
<h1 id="hName">PanelSat</h1><p class="sub" id="hSub">…</p>
<div class="chips" id="chips"></div>

<div class="card" id="authCard" style="display:none"><h2 id="authT">Anmelden</h2><p class="hint" id="authH"></p>
<div id="pwSetup"><label class="f"><span>Standardpasswort (steht in der Anleitung)</span><input id="pOld" type="password" autocomplete="current-password"></label>
<label class="f"><span>Neues Passwort (mindestens 8 Zeichen)</span><input id="pNew" type="password" autocomplete="new-password"></label>
<label class="f"><span>Neues Passwort wiederholen</span><input id="pNew2" type="password" autocomplete="new-password"></label></div>
<div id="pwLogin"><label class="f"><span>Passwort</span><input id="pLogin" type="password" autocomplete="current-password"></label></div>
<div class="row"><button class="p" id="bAuth" style="flex:none">Weiter</button></div></div>
<div id="app" style="display:none">

<div class="card"><h2>Satellit</h2>
<div class="row"><label class="f"><span>Name</span><input id="cName" maxlength="32"></label>
<label class="f"><span>Kennung (Teil der Topics)</span><input id="cId" maxlength="32" class="mono"></label></div>
<h2 style="margin-top:6px">WLAN</h2>
<div class="row"><label class="f"><span>Netzname (SSID)</span><input id="cSsid" maxlength="32"></label>
<label class="f"><span>Passwort</span><input id="cWpass" type="password" maxlength="64" placeholder="unverändert"></label></div>
<div id="netList"></div>
<div class="row" style="flex:none"><button id="bScanW" style="flex:none">Netze suchen</button></div>
<p class="hint">Ohne WLAN-Verbindung öffnet der Satellit nach 30 s das Einrichtungs-WLAN „SOC-Satellite-…“, die Einstellungsseite erscheint dann von selbst (sonst http://4.3.2.1/).</p>
<h2 style="margin-top:14px">Einrichtungs-WLAN</h2>
<p class="hint">Das eigene WLAN „<span class="apSsid">…</span>“ zum Einrichten (öffnet sich, wenn keine WLAN-Verbindung besteht). Sein WLAN-Passwort ist unabhängig vom Zugangspasswort dieser Seite.</p>
<label class="ck"><input type="checkbox" id="apOn">Mit WLAN-Passwort schützen (ab Werk: <b>panelsat</b>)</label>
<div class="row" id="apPwRow"><label class="f"><span>WLAN-Passwort (mindestens 8 Zeichen)</span><input id="apPw" type="password" maxlength="63" placeholder="unverändert" autocomplete="new-password"></label></div>
<p class="hint" id="apOpenHint" style="display:none">Ohne WLAN-Passwort kann sich jeder in der Nähe mit dem Einrichtungs-WLAN verbinden – Einstellungen ändern geht trotzdem nur mit dem Zugangspasswort.</p>
<div class="row"><button id="bApSave" style="flex:none">WLAN-Passwort speichern</button><button id="bPw" style="flex:none">Zugangspasswort ändern</button></div>
<p class="hint">Notfall: 3× kurz hintereinander aus- und einschalten (je unter 10 s) öffnet das Einrichtungs-WLAN sofort, 5× setzt WLAN-Verbindung, Zugangspasswort und WLAN-Passwort zurück (beide wieder <b>panelsat</b>).</p>

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
<label class="ck"><input type="checkbox" id="mUdp">UDP direkt ans Panel (im WLAN, ohne Broker)</label>
<div id="udpBox" style="margin-left:28px"><label class="f"><span>Panel-IP (leer = an alle im Netz)</span><input id="uIp" placeholder="leer = Broadcast" class="mono"></label></div>
<div id="bleBox"><label class="f"><span>Satelliten-Passwort (wie im Panel unter Einstellungen → Geräte → Satelliten, verschlüsselt die Pakete)</span><input id="sBlePw" maxlength="32" autocomplete="off"></label>
<p class="hint" id="bleName"></p></div>

<h2 style="margin-top:14px" id="bleH">Bluetooth-Geräte auswerten</h2>
<label class="ck"><input type="checkbox" id="pTruma">Truma LevelControl</label>
<label class="ck"><input type="checkbox" id="pMopeka">Mopeka</label>
<label class="ck"><input type="checkbox" id="pVictron">Victron (Instant Readout)</label>
<label class="ck" id="pHymerL" style="display:none"><input type="checkbox" id="pHymer">Hymer Connect (SCU im Fahrzeug)</label>
<div class="row" style="margin-top:10px"><button class="p" id="bSave" style="flex:none">Speichern</button></div>
</div>

<div class="card" id="ioCard"><h2>Anschlüsse (GPIO)</h2>
<p class="hint">Hier legst du fest, was an den Pins des Satelliten hängt. Die Werte gehen auf denselben Wegen ans Panel wie die Bluetooth-Geräte (MQTT mit Home Assistant, UDP, Bluetooth).</p>
<div class="row"><label class="f"><span>Sensoren abfragen alle … Sekunden</span><input id="ioInt" type="number" min="2" max="3600"></label>
<label class="f"><span>Tanksonden abfragen alle … Sekunden</span><input id="ioTInt" type="number" min="2" max="65535"></label></div>
<div id="ioBody"><p class="hint">Lädt …</p></div>
<div class="row" style="margin-top:10px"><button class="p" id="bIoSave" style="flex:none">Belegung speichern</button><button id="bIoTank" style="flex:none">Tanks jetzt messen</button></div>
</div>

<div class="card" id="trCard"><h2>Truma LevelControl</h2><div id="trList"></div>
<div class="row" style="margin-top:8px"><label class="f" style="flex:none;width:200px"><span>Messen alle … Minuten</span><input id="trInt" type="number" min="1" max="1440"></label></div>
<div class="row"><button id="bTrScan" style="flex:none">Sensoren suchen</button><button id="bTrMeasure" style="flex:none">Jetzt messen</button></div>
<div id="trFound"></div></div>

<div class="card" id="moCard"><h2>Mopeka</h2><div id="moList"></div>
<div class="row" style="margin-top:8px"><button id="bMoScan" style="flex:none">Sensoren suchen</button></div>
<div id="moFound"></div></div>

<div class="card" id="viCard"><h2>Victron</h2>
<p class="hint">Pro Gerät den Schlüssel aus VictronConnect: Einstellungen → Produktinfo → Instant Readout → Details anzeigen.</p>
<div id="viList"></div>
<div class="row" style="margin-top:8px"><button id="bViScan" style="flex:none">Geräte suchen</button></div>
<div id="viFound"></div></div>

<div class="card" id="hyCard" style="display:none"><h2>Hymer Connect</h2>
<p class="hint">Liest Licht, Batterie, Tanks und Schalter direkt von der SCU im Hymer (bzw. Bürstner, Dethleffs, Eriba, LMC …) – ohne Cloud. Koppeln: hier „Koppeln“ drücken, dann am Bedienteil im Fahrzeug „CONNECTION“ drücken (3 Minuten Zeit). Die Werte gehen wie die anderen Anschlüsse ans Panel.</p>
<p class="hint" id="hyStatus"></p>
<div id="hyList"></div>
<div class="row" style="margin-top:8px"><button id="bHyPair" style="flex:none">Koppeln</button><button class="d" id="bHyForget" style="flex:none">Kopplung lösen</button></div></div>

<div class="card" id="rdCard" style="display:none"><h2>Kacheln vom Hauptpanel</h2>
<p class="hint">Welche Kacheln dieses Display zeigt. „Geräteliste holen“ fragt das Hauptpanel nach allen Kacheln – die Liste wird hier nicht gespeichert, gemerkt wird nur deine Auswahl (im Hauptpanel). Anordnen auf Seiten: Karte „Display“ darunter.</p>
<div class="row"><button id="bRdLoad" style="flex:none">Geräteliste vom Hauptpanel holen</button></div>
<div id="rdList"></div>
<div class="row" id="rdSaveRow" style="display:none;margin-top:8px"><button class="p" id="bRdSave" style="flex:none">Auswahl speichern</button></div></div>

<div class="card" id="dpCard" style="display:none"><h2>Display</h2>
<p class="hint">Welche Werte das Display zeigt, in welcher Reihenfolge und unter welchem Namen. Neue Geräte erscheinen automatisch hinten.</p>
<p class="hint">Vorschau des Displays. Kachel antippen, dann den Zielplatz antippen (belegt = tauschen, frei = verschieben) – am PC auch ziehen. Je Seite 1, 2 oder 4 Kacheln, weitere Seiten mit „+ Seite hinzufügen“. Erst „Speichern“ übernimmt es aufs Display.</p>
<div id="dpEd"></div><div id="dpSel"></div><div id="dpHid"></div>
<div class="row" style="margin-top:10px">
<label class="f"><span>Kacheln je Seite für neue Seiten</span><select id="dpPer"><option value="4">4 (2 × 2)</option><option value="2">2 (groß)</option><option value="1">1 (ganzer Bildschirm)</option></select></label>
<label class="f"><span>Helligkeit</span><input type="range" id="dpBright" min="20" max="255"></label>
<label class="f" id="dpLedL" style="display:none"><span>LED-Ampel auf der Rückseite</span><select id="dpLed"></select></label>
<label class="f" id="dpSvL" style="display:none"><span>Nach der Dimm-Zeit</span><select id="dpSv"><option value="0">nur abdunkeln</option><option value="1">Uhr zeigen (Zeit vom Hauptpanel)</option></select></label>
<label class="f"><span>Dimmen nach</span><select id="dpDim"><option value="1">1 Minute</option><option value="2">2 Minuten</option><option value="5">5 Minuten</option><option value="10">10 Minuten</option><option value="0">nie</option></select></label>
<label class="ck"><input type="checkbox" id="dpAuto">Helligkeit automatisch (Lichtsensor vorne am Display)</label>
<label class="f"><span>Helligkeit bei Dunkelheit</span><input type="range" id="dpNight" min="5" max="255"></label>
<label class="f"><span>Dunkel ab Messwert</span><input type="number" id="dpThr" min="1" max="4095"></label>
<p class="hint"><span id="ldrNow">Lichtsensor: …</span> Größerer Wert = dunkler. Zum Einstellen: Raum abdunkeln, Wert ablesen und etwas darunter als Grenze eintragen.</p>
</div>
<div class="row" style="margin-top:8px"><button id="bDpSave" style="flex:none">Speichern</button></div></div>

<div class="card" id="olCard" style="display:none"><h2>OLED</h2>
<p class="hint">Was das kleine Display zeigt. <b>Automatisch</b>: mit Lagesensor (MPU-6050) die Wasserwaage in zwei Richtungen, sonst alle Sensoren nacheinander, ohne Sensor Name und Adresse des Satelliten. Der MPU-6050 hängt am selben I²C wie das OLED (SDA GPIO <span id="olSda"></span>, SCL GPIO <span id="olScl"></span>). Läuft die Blase falsch herum: unter Anschlüsse beim MPU-6050 die Achsen tauschen bzw. umkehren.</p>
<p class="hint" id="olMiss" style="display:none;color:#e5534b">Das OLED antwortet nicht – Anschluss bzw. Board prüfen.</p>
<div class="row">
<label class="f"><span>Anzeige</span><select id="olSrc"></select></label>
<label class="f"><span>Helligkeit (Kontrast)</span><input type="range" id="olCon" min="1" max="255"></label>
</div>
<label class="ck"><input type="checkbox" id="olOn">Display an</label>
<label class="ck"><input type="checkbox" id="olRot">Um 180° drehen</label>
<div class="row" style="margin-top:8px"><button id="bOlSave" style="flex:none">Speichern</button></div></div>
<div class="card"><h2>Firmware</h2>
<p class="hint" id="fwInfo"></p>
<div id="updRow" style="display:none"><p class="hint">Update: „Update-Modus starten“ – das Gerät startet in den kleinen Updater (gleiche Adresse, gleiches Passwort). Dort die neue Firmware hochladen; danach startet sie von selbst. Hier unten lässt sich nur der Updater selbst erneuern (max. 1 MB).</p>
<div class="row"><button id="bUpd" style="flex:none">Update-Modus starten</button></div></div>
<div class="row"><input type="file" id="fwFile" accept=".bin"><button id="bFw" style="flex:none">Hochladen</button><button id="bReboot" style="flex:none">Neu starten</button></div>
</div>
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
 $('#mMqtt').checked=S.modes.mqtt;$('#mUdp').checked=S.modes.udp;
 $('#qHost').value=S.mqtt.host;$('#qPort').value=S.mqtt.port;$('#qUser').value=S.mqtt.user;$('#qBase').value=S.mqtt.base;
 $('#qHa').value=S.mqtt.ha_prefix;$('#qHaOn').checked=S.mqtt.ha;$('#qPass').placeholder=S.mqtt.has_pass?'unverändert':'kein Passwort';
 $('#uIp').value=S.udp_target;$('#sBlePw').value=S.ble_pw;$('#pTruma').checked=S.ble.truma;$('#pMopeka').checked=S.ble.mopeka;$('#pVictron').checked=S.ble.victron;if('hymer' in S.ble){$('#pHymerL').style.display='';$('#pHymer').checked=S.ble.hymer}
 $('#trInt').value=S.truma.interval;boxes();
}
function boxes(){$('#mqttBox').style.display=$('#mMqtt').checked?'':'none';$('#udpBox').style.display=$('#mUdp').checked?'':'none'}
function render(){
 $('#hName').textContent=S.name;document.title=S.name+' – PanelSat';
 $('#hSub').textContent='Kennung '+S.id+(S.wifi.ip?' · '+S.wifi.ip:'');
 const c=[];c.push(S.wifi.connected?['ok','WLAN '+S.wifi.ssid+' ('+S.wifi.rssi+' dBm)']:['err',S.wifi.ssid?'WLAN getrennt':'kein WLAN eingerichtet']);
 if(S.wifi.ap)c.push(['','Einrichtungs-WLAN '+S.wifi.ap_ssid]);
 if(S.modes.mqtt)c.push([S.mqtt.connected?'ok':'err','MQTT: '+S.mqtt.status]);
 if(S.modes.udp&&!S.remote)c.push(['ok','UDP: '+S.udp_sent+' Pakete']);
 if(S.remote){c.push([/^verbunden/.test(S.rd)?'ok':'err','Hauptpanel: '+S.rd]);$('#rdCard').style.display='';
  ['#ioCard','#trCard','#moCard','#viCard','#bleH'].forEach(s=>{const e=$(s);if(e)e.style.display='none'});
  ['#pTruma','#pMopeka','#pVictron','#mUdp'].forEach(s=>{const e=$(s);if(e)e.closest('label').style.display='none'});
  const ub=$('#udpBox');if(ub){ub.style.marginLeft='0';const sp=ub.querySelector('span');if(sp)sp.textContent='IP des Hauptpanels (wird beim Anlernen eingetragen und automatisch nachgeführt)'}}

 $('#chips').innerHTML=c.map(x=>`<span class="chip ${x[0]}">${esc(x[1])}</span>`).join('');
 $('#bleName').textContent='Per Bluetooth heißt dieser Satellit „'+S.ble_name+'“. Das Panel nimmt ihn automatisch auf, wenn das Passwort passt.';
 $('#fwInfo').textContent='Version '+S.fw+(S.chip?' ('+S.chip+')':'')+', freier Speicher '+Math.round(S.heap/1024)+' KB, läuft seit '+Math.round(S.uptime/60)+' Min.'+(S.upd_slot?(S.upd_ok?' · Updater vorhanden':' · Updater fehlt (einmal per USB aufspielen)'):'');$('#updRow').style.display=S.upd_slot?'':'none';$('#bUpd').disabled=!S.upd_ok;
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
async function apLoad(){try{const r=await fetch('/api/appw');if(!r.ok)return;const j=await r.json();$('#apOn').checked=j.on;apShow();document.querySelectorAll('.apSsid').forEach(e=>e.textContent=j.ssid||'…');$('#apPw').placeholder=j.custom?'unverändert':'unverändert (panelsat)'}catch(e){}}
function apShow(){$('#apPwRow').style.display=$('#apOn').checked?'':'none';$('#apOpenHint').style.display=$('#apOn').checked?'none':''}
$('#apOn').onchange=apShow;
$('#bApSave').onclick=()=>{const pw=$('#apPw').value;if($('#apOn').checked&&pw&&pw.length<8)return toast('Das WLAN-Passwort braucht mindestens 8 Zeichen.',true);
 post('/api/appw',{on:$('#apOn').checked,pw:pw}).then(()=>{$('#apPw').value='';toast('Gespeichert – das Einrichtungs-WLAN startet gleich mit der neuen Einstellung neu.');apLoad()}).catch(e=>toast(e.message,true))};
let AUTH=null;
function showAuth(mode){AUTH=mode;$('#app').style.display='none';$('#authCard').style.display='';
 const setup=mode==='setup';$('#pwSetup').style.display=setup?'':'none';$('#pwLogin').style.display=setup?'none':'';
 $('#authT').textContent=setup?'Erste Einrichtung: Passwort vergeben':'Anmelden';
 $('#authH').textContent=setup?'Zum Schutz zuerst das Standardpasswort eingeben und ein eigenes Zugangspasswort für diese Seite vergeben. Das WLAN-Passwort des Einrichtungs-WLANs stellst du danach getrennt ein.':'Passwort des Satelliten eingeben.';$('#hSub').textContent=''}
$('#bAuth').onclick=async()=>{try{
 if(AUTH==='setup'){if($('#pNew').value!==$('#pNew2').value)return toast('Die neuen Passwörter sind nicht gleich.',true);
  await post('/api/setpw',{old:$('#pOld').value,new:$('#pNew').value});toast('Zugangspasswort gespeichert.')}
 else await post('/api/login',{pw:$('#pLogin').value});
 setTimeout(()=>location.reload(),800)}catch(e){toast(e.message,true)}};
$('#bPw').onclick=async()=>{const o=prompt('Aktuelles Zugangspasswort');if(o==null)return;const n=prompt('Neues Passwort (mindestens 8 Zeichen)');if(!n)return;if(prompt('Neues Passwort wiederholen')!==n)return toast('Nicht gleich.',true);
 try{await post('/api/setpw',{old:o,new:n});toast('Passwort geändert.')}catch(e){toast(e.message,true)}};
$('#netList').addEventListener('click',e=>{const b=e.target.closest('button');if(!b)return;const n=$('#netList')._n[+b.dataset.net];$('#cSsid').value=n.ssid;$('#netList').innerHTML='';$('#cWpass').value='';$('#cWpass').focus();toast('„'+n.ssid+'“ gewählt – Passwort eingeben und speichern.')});
async function load(){try{const r=await fetch('/api/state');if(r.status===401){const j=await r.json();if(AUTH!==j.auth)showAuth(j.auth);return}
 AUTH=null;$('#authCard').style.display='none';$('#app').style.display='';S=await r.json();if(!loaded){fill();apLoad();loaded=true}render();ioLive();ldrShow()}catch(e){$('#hSub').textContent='Keine Verbindung zum Satelliten …'}}
// ---------------- Anschluesse (GPIO)
let IO=null,PINS=[];
const CLS=[['','Allgemein'],['temperature','Temperatur'],['humidity','Luftfeuchte'],['level','Füllstand'],['voltage','Spannung'],['pressure','Druck'],['current','Strom']];
const P5=[0,1,2,3,4];
function ioUsed(){const u=new Set(),C=IO;C.tanks.forEach(t=>{if(t.mode==1){t.pins.forEach(p=>p>=0&&u.add(p));t.gnd>=0&&u.add(t.gnd)}if(t.mode==2&&t.apin>=0)u.add(t.apin)});if(C.ow_pin>=0)u.add(C.ow_pin);C.analog.forEach(a=>a.pin>=0&&u.add(a.pin));if(C.mpu.on){u.add(C.mpu.sda);u.add(C.mpu.scl)}C.switches.forEach(x=>x.pin>=0&&u.add(x.pin));C.inputs.forEach(x=>x.pin>=0&&u.add(x.pin));return u}
const fits=(p,need)=>(need!='out'||p.out)&&(need!='adc'||p.adc);
function freePin(need){const u=ioUsed();const f=PINS.filter(p=>!u.has(p.pin)&&fits(p,need));return (f.find(p=>!p.note)||f[0]||{pin:-1}).pin}
function pinSel(k,v,need,opt){const u=ioUsed();return `<select data-k="${k}" data-n data-r>${opt?`<option value="-1" ${v<0?'selected':''}>– keiner –</option>`:''}${PINS.filter(p=>fits(p,need)).map(p=>`<option value="${p.pin}" ${p.pin==v?'selected':''}>${p.label||'GPIO '+p.pin}${u.has(p.pin)&&p.pin!=v?' (belegt)':''}${p.note?' – '+esc(p.note):''}</option>`).join('')}</select>`}
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
  <div class="row">${fld('Name',inp(b+'.name',x.name,'maxlength="32" placeholder="z. B. Wasserpumpe"'))}${fld('Ausgang',pinSel(b+'.pin',x.pin,'out'))}</div>
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
$('#bIoSave').onclick=()=>post('/api/io',IO).then(()=>{toast('Belegung gespeichert.');ioLoad();olLoad();setTimeout(load,800)}).catch(e=>toast(e.message,true));
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
$('#mMqtt').onchange=$('#mUdp').onchange=boxes;
$('#bSave').onclick=()=>{
 const d={name:$('#cName').value.trim(),id:$('#cId').value.trim(),wifi:{ssid:$('#cSsid').value.trim()},
  mqtt:{host:$('#qHost').value.trim(),port:+$('#qPort').value||1883,user:$('#qUser').value.trim(),base:$('#qBase').value.trim(),ha:$('#qHaOn').checked,ha_prefix:$('#qHa').value.trim()},
  modes:{mqtt:$('#mMqtt').checked,udp:$('#mUdp').checked},udp_target:$('#uIp').value.trim(),ble_pw:$('#sBlePw').value.trim(),
  ble:{truma:$('#pTruma').checked,mopeka:$('#pMopeka').checked,victron:$('#pVictron').checked,...('hymer' in S.ble?{hymer:$('#pHymer').checked}:{})}};
 if($('#cWpass').value)d.wifi.pass=$('#cWpass').value;if($('#qPass').value)d.mqtt.pass=$('#qPass').value;
 post('/api/config',d).then(r=>{toast(r.restart?'Gespeichert – der Satellit startet neu und verbindet sich mit dem WLAN.':'Gespeichert.');$('#cWpass').value='';$('#qPass').value='';loaded=false;setTimeout(load,r.restart?8000:500)}).catch(e=>toast(e.message,true));
};
$('#bScanW').onclick=async()=>{$('#bScanW').disabled=true;$('#bScanW').textContent='Suche …';try{const r=await fetch('/api/wifiscan');const j=await r.json();if(!r.ok)throw new Error(j.error||'Suche fehlgeschlagen');
 $('#netList').innerHTML=j.networks.length?j.networks.map((n,i)=>`<div class="dev"><div class="t">${esc(n.ssid)}<div class="v">Signal ${sig(n.rssi)}${n.open?' · offen':''}</div></div><button data-net="${i}" style="flex:none">Wählen</button></div>`).join(''):'<p class="hint">Keine Netze gefunden.</p>';
 $('#netList')._n=j.networks}catch(e){toast(e.message,true)}$('#bScanW').disabled=false;$('#bScanW').textContent='Netze suchen'};
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
$('#bUpd').onclick=async()=>{if(!confirm('In den Update-Modus starten? Das Gerät ist bis zum Update (oder 15 Minuten) nicht erreichbar wie gewohnt.'))return;
 try{await post('/api/updater',{});toast('Starte den Updater … die Seite lädt gleich neu.');setTimeout(()=>location.reload(),12000)}catch(e){toast(e.message,true)}};
$('#bFw').onclick=async()=>{const f=$('#fwFile').files[0];if(!f)return toast('Erst eine .bin-Datei wählen',true);if(S.upd_slot&&f.size>S.upd_slot)return toast('Zu groß für den Updater-Bereich – die Firmware bitte über den Update-Modus aufspielen.',true);const fd=new FormData();fd.append('fw',f);$('#bFw').disabled=true;$('#bFw').textContent='Lädt hoch …';
 try{const r=await fetch('/api/update?size='+f.size,{method:'POST',body:fd});const j=await r.json();if(j.ok){toast('Firmware übernommen – Neustart …');setTimeout(()=>location.reload(),9000)}else toast(j.error||'Fehlgeschlagen',true)}catch(e){toast('Hochladen fehlgeschlagen',true)}$('#bFw').disabled=false;$('#bFw').textContent='Hochladen'};
$('#bReboot').onclick=()=>confirm('Satellit neu starten?')&&act('/api/reboot',{},'Startet neu …');
let RDA=null;   // Kachelliste vom Hauptpanel (nur im Browser, wird nicht gespeichert)
function rdRender(){if(!RDA)return;const g=[...new Set(RDA.avail.map(x=>x.p))];
 $('#rdList').innerHTML=RDA.avail.length?g.map(p=>`<h2 style="margin:10px 0 4px;font-size:14px">${esc(p)}</h2>`+RDA.avail.filter(x=>x.p===p).map(x=>`<label class="ck"><input type="checkbox" data-rdk="${esc(x.k)}" ${RDA.sel.has(x.k)?'checked':''}>${esc(x.n)}</label>`).join('')).join(''):'<p class="hint">Das Hauptpanel hat keine Kacheln.</p>';
 $('#rdSaveRow').style.display=''}
$('#bRdLoad').onclick=async()=>{$('#bRdLoad').disabled=true;try{const r=await fetch('/api/rd/avail');const j=await r.json();if(!r.ok||j.error)throw new Error(j.error||r.statusText);RDA={avail:j.avail||[],order:j.tiles||[],sel:new Set(j.tiles||[])};rdRender()}catch(e){toast(e.message,true)}$('#bRdLoad').disabled=false};
$('#rdList').addEventListener('change',e=>{const k=e.target.dataset.rdk;if(k==null)return;if(e.target.checked)RDA.sel.add(k);else RDA.sel.delete(k)});
$('#bRdSave').onclick=()=>{if(!RDA)return;const tiles=RDA.order.filter(k=>RDA.sel.has(k)).concat(RDA.avail.map(x=>x.k).filter(k=>RDA.sel.has(k)&&!RDA.order.includes(k)));
 post('/api/rd/sel',{tiles}).then(()=>{RDA.order=tiles;toast('Gespeichert – die Kacheln kommen gleich.');setTimeout(dpLoad,3000)}).catch(e=>toast(e.message,true))};
let DP=null,dSel=null,dG=null;
function ldrShow(){const l=S.light;if(!l||l.ldr<0)return;$('#ldrNow').textContent='Lichtsensor zurzeit: '+l.ldr+' – '+(l.night?'dunkel':'hell')+(l.auto?'.':' (Automatik aus).')}
const dPl=p=>p<DP.pl.length?DP.pl[p]:DP.per,dT=k=>DP.tiles.find(t=>t.k==k);
// Seiten aufbauen wie das Display: feste Seite aus den Einstellungen, der Rest fliesst hinten auf neue Seiten
function dpBuild(){const P=[],au=[];DP.tiles.filter(t=>t.on).forEach(t=>{if(t.pg!=null&&t.pg<16){while(P.length<=t.pg)P.push({n:dPl(P.length),k:[]});P[t.pg].k.push(t.k)}else au.push(t.k)});
 P.forEach(p=>au.push(...p.k.splice(p.n)));for(let i=0;i<au.length;){const n=dPl(P.length);P.push({n,k:au.slice(i,i+n)});i+=n}
 if(!P.length)P.push({n:dPl(0),k:[]});DP.P=P}
async function dpLoad(){try{const r=await fetch('/api/display');if(!r.ok)return;DP=await r.json();DP.pl=DP.pl||[];dSel=null;dpBuild();dpRender()}catch(e){}}
// zu viele Kacheln auf einer Seite: Ueberzaehlige an den Anfang der naechsten Seite
function dpFix(){const P=DP.P;for(let p=0;p<P.length;p++){const x=P[p].k.splice(P[p].n);if(x.length){if(!P[p+1])P.push({n:DP.per,k:[]});P[p+1].k.unshift(...x)}}}
// Kachel von Platz a nach Platz b: belegt = tauschen, frei = verschieben
function dpMove(a,b){const P=DP.P,ka=P[a.p].k[a.i];if(ka==null||(a.p==b.p&&a.i==b.i))return;const kb=P[b.p].k[b.i];
 if(kb!=null){P[a.p].k[a.i]=kb;P[b.p].k[b.i]=ka}else{P[a.p].k.splice(a.i,1);P[b.p].k.push(ka)}}
const dFlat=()=>{const f=[];DP.P.forEach((p,pi)=>p.k.forEach((k,i)=>f.push({p:pi,i})));return f};
function dpRender(){if(!DP)return;$('#dpCard').style.display='';let h='';
 DP.P.forEach((pg,p)=>{let c='';for(let i=0;i<pg.n;i++){const k=pg.k[i],t=k!=null&&dT(k),sel=dSel&&dSel.p==p&&dSel.i==i;
   c+=t?`<div class="tl${sel?' sel':''}" draggable="true" data-p="${p}" data-i="${i}"><div class="tn">${esc(t.n||t.name)}</div><div class="tv">${esc((t.value||'–').split('  ·  ')[0])}</div></div>`:`<div class="tl em" data-p="${p}" data-i="${i}"></div>`}
  h+=`<div class="dpp"><div class="dph"><b>Seite ${p+1}</b>${!pg.k.length&&DP.P.length>1?`<button class="s d" data-dpdel="${p}">Seite löschen</button>`:''}<select data-pl="${p}">${[4,2,1].map(v=>`<option value="${v}"${v==pg.n?' selected':''}>${v} ${v==1?'Kachel':'Kacheln'}</option>`).join('')}</select></div><div class="scr n${pg.n}">${c}</div></div>`});
 $('#dpEd').innerHTML=(DP.tiles.length?h:'<p class="hint">Noch keine Werte – erst Geräte oder Anschlüsse einrichten.</p>')+(DP.tiles.length?'<button id="dpAdd" style="margin:0 0 12px">+ Seite hinzufügen</button>':'');
 const sk=dSel&&DP.P[dSel.p].k[dSel.i],t=sk!=null&&sk!==undefined&&dT(sk),F=dFlat(),fi=dSel?F.findIndex(x=>x.p==dSel.p&&x.i==dSel.i):-1;
 $('#dpSel').innerHTML=t?`<div class="io"><div class="ioh"><b>${esc(t.name)}</b><button class="s" data-dmv="-1" ${fi>0?'':'disabled'}>◀</button><button class="s" data-dmv="1" ${fi>=0&&fi<F.length-1?'':'disabled'}>▶</button></div><label class="f"><span>Eigener Name (leer = ${esc(t.name)})</span><input id="dpName" maxlength="32" value="${esc(t.n||'')}"></label><div class="row"><button class="d" id="dpHide" style="flex:none">Ausblenden</button><button id="dpDone" style="flex:none">Fertig</button></div></div>`:'';
 const H=DP.tiles.filter(t=>!t.on);
 $('#dpHid').innerHTML=H.length?'<h2 style="margin-top:6px">Ausgeblendet</h2>'+H.map(t=>`<div class="dev"><div class="t">${esc(t.n||t.name)}<div class="v">${esc(t.value||'')}</div></div><button data-dshow="${esc(t.k)}">Einblenden</button></div>`).join(''):'';
 $('#dpSvL').style.display=DP.clock?'':'none';
 if(DP.led_tiles){$('#dpLedL').style.display='';$('#dpLed').innerHTML='<option value="0">aus</option><option value="1">automatisch (niedrigster Pegel)</option>'+DP.led_tiles.map(t=>`<option value="k:${esc(t.k)}">${esc(t.n)}</option>`).join('');$('#dpLed').value=DP.led==2?'k:'+DP.led_key:String(DP.led)}$('#dpSv').value=DP.saver||0;$('#dpPer').value=DP.per;$('#dpBright').value=DP.bright;$('#dpDim').value=DP.dim;$('#dpAuto').checked=DP.auto;$('#dpNight').value=DP.night;$('#dpThr').value=DP.ldr_thr;
 if(DP.round){document.querySelectorAll('#dpEd .dph select,#dpAdd').forEach(x=>x.style.display='none');['#dpPer','#dpAuto','#dpNight','#dpThr'].forEach(q=>{const e=$(q);if(e)(e.closest('label')||e).style.display='none'});const l=$('#ldrNow');if(l)l.parentNode.style.display='none'}}
$('#dpEd').addEventListener('click',e=>{const b=e.target.closest('button');
 if(b&&b.id=='dpAdd'){DP.P.push({n:DP.per,k:[]});dSel=null;return dpRender()}
 if(b&&b.dataset.dpdel){DP.P.splice(+b.dataset.dpdel,1);dSel=null;return dpRender()}
 const el=e.target.closest('.tl');if(!el)return;const at={p:+el.dataset.p,i:+el.dataset.i},has=DP.P[at.p].k[at.i]!=null;
 if(!dSel){if(has)dSel=at}else if(dSel.p==at.p&&dSel.i==at.i)dSel=null;else{dpMove(dSel,at);dSel=null}dpRender()});
$('#dpEd').addEventListener('change',e=>{const d=e.target.dataset;if(d.pl==null)return;DP.P[+d.pl].n=+e.target.value;dpFix();dSel=null;dpRender()});
$('#dpEd').addEventListener('dragstart',e=>{const el=e.target.closest('.tl');if(!el||el.classList.contains('em'))return;dG={p:+el.dataset.p,i:+el.dataset.i};e.dataTransfer.effectAllowed='move';e.dataTransfer.setData('text/plain','x')});
$('#dpEd').addEventListener('dragover',e=>{const el=e.target.closest('.tl');if(!dG||!el)return;e.preventDefault();document.querySelectorAll('.tl.ov').forEach(x=>x.classList.remove('ov'));el.classList.add('ov')});
$('#dpEd').addEventListener('drop',e=>{const el=e.target.closest('.tl');if(!el||!dG)return;e.preventDefault();dpMove(dG,{p:+el.dataset.p,i:+el.dataset.i});dG=null;dSel=null;dpRender()});
$('#dpEd').addEventListener('dragend',()=>{dG=null;document.querySelectorAll('.tl.ov').forEach(x=>x.classList.remove('ov'))});
$('#dpSel').addEventListener('click',e=>{const b=e.target.closest('button');if(!b||!dSel)return;
 if(b.dataset.dmv){const F=dFlat(),fi=F.findIndex(x=>x.p==dSel.p&&x.i==dSel.i),to=F[fi+(+b.dataset.dmv)];if(to){dpMove(dSel,to);dSel=to}}
 if(b.id=='dpHide'){const k=DP.P[dSel.p].k.splice(dSel.i,1)[0];dT(k).on=false;dSel=null}
 if(b.id=='dpDone')dSel=null;dpRender()});
$('#dpSel').addEventListener('input',e=>{if(e.target.id!='dpName'||!dSel)return;const t=dT(DP.P[dSel.p].k[dSel.i]);if(t)t.n=e.target.value});
$('#dpHid').addEventListener('click',e=>{const b=e.target.closest('button');if(!b||!b.dataset.dshow)return;const t=dT(b.dataset.dshow);if(!t)return;t.on=true;
 let L=DP.P[DP.P.length-1];if(L.k.length>=L.n){L={n:DP.per,k:[]};DP.P.push(L)}L.k.push(t.k);dpRender()});
$('#dpPer').onchange=e=>{DP.per=+e.target.value};
// Speichern: leere Seiten fallen weg, jede Kachel bekommt ihre Seite
$('#bDpSave').onclick=()=>{const P=DP.P.filter(p=>p.k.length),tiles=[];P.forEach((p,pi)=>p.k.forEach(k=>{const t=dT(k);tiles.push({k,on:true,n:t.n||'',pg:pi})}));
 DP.tiles.filter(t=>!t.on).forEach(t=>tiles.push({k:t.k,on:false,n:t.n||''}));
 post('/api/display',{tiles,pl:P.map(p=>p.n),saver:+$('#dpSv').value,...(DP.led_tiles?{led:$('#dpLed').value.startsWith('k:')?2:+$('#dpLed').value,led_key:$('#dpLed').value.startsWith('k:')?$('#dpLed').value.slice(2):''}:{}),per:+$('#dpPer').value,bright:+$('#dpBright').value,dim:+$('#dpDim').value,auto:$('#dpAuto').checked,night:+$('#dpNight').value,ldr_thr:+$('#dpThr').value}).then(()=>{toast('Display gespeichert.');dpLoad()}).catch(e=>toast(e.message,true))};
async function olLoad(){try{const r=await fetch('/api/oled');if(!r.ok)return;const o=await r.json();$('#olCard').style.display='';$('#olSda').textContent=o.sda;$('#olScl').textContent=o.scl;$('#olMiss').style.display=o.present?'none':'';
 $('#olSrc').innerHTML='<option value="">Automatisch</option><option value="level">Wasserwaage (MPU-6050)</option>'+(o.pts||[]).map(p=>`<option value="${esc(p.id)}">${esc(p.name)}</option>`).join('');$('#olSrc').value=o.src||'';if($('#olSrc').value!==(o.src||''))$('#olSrc').value='';$('#olCon').value=o.con;$('#olOn').checked=o.on;$('#olRot').checked=o.rot}catch(e){}}
$('#bOlSave').onclick=()=>post('/api/oled',{src:$('#olSrc').value,con:+$('#olCon').value,on:$('#olOn').checked,rot:$('#olRot').checked}).then(()=>toast('OLED gespeichert.')).catch(e=>toast(e.message,true));
load();ioLoad();dpLoad();olLoad();setInterval(load,3000);
</script></body></html>)HTML";
