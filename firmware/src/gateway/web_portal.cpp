// ============================================================
// Datei:   web_portal.cpp
// Projekt: Briefkastensensor (C++ Portierung von MicroPython)
// Autor:   Jonas Thern
// Hinweis: Kommentare und Formatierung wurden mit Unterstuetzung
//          von kuenstlicher Intelligenz (Claude, Anthropic) erstellt.
// ============================================================

#include "web_portal.h"
#include <WiFi.h>
#include <ESPmDNS.h>

static const char* MDNS_HOSTNAME = "briefkastensensor";

// ------------------------------------------------------------
// HTML-Oberflaeche: eine Seite, Status per JSON alle 5 Sekunden.
// Umlaute als HTML-Entities, damit der Quelltext reines ASCII bleibt.
// ------------------------------------------------------------
static const char PAGE_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Briefkastensensor</title>
<style>
 body{font-family:sans-serif;max-width:520px;margin:20px auto;padding:0 12px;background:#f4f4f4;color:#222}
 h1{font-size:1.3em}h2{font-size:1.05em;margin-top:24px}
 .card{background:#fff;border-radius:8px;padding:14px;margin-bottom:12px;box-shadow:0 1px 3px rgba(0,0,0,.15)}
 .row{display:flex;justify-content:space-between;padding:3px 0}
 .val{font-weight:bold}
 .ok{color:#1a7f37}.warn{color:#b35c00}.err{color:#c00}
 button{background:#2b6cb0;color:#fff;border:none;border-radius:6px;padding:9px 14px;font-size:1em;cursor:pointer}
 button:disabled{background:#999}
 input{width:100%;box-sizing:border-box;padding:7px;margin:3px 0 10px;border:1px solid #bbb;border-radius:5px}
 label{font-size:.9em;color:#555}
 .note{font-size:.8em;color:#777}
</style>
</head>
<body>
<h1>Briefkastensensor Gateway</h1>

<div class="card">
 <h2 style="margin-top:0">Sendemodul</h2>
 <div class="row"><span>Briefkasten</span><span class="val" id="mail">-</span></div>
 <div class="row"><span>Akku-Warnflag</span><span class="val" id="batt">-</span></div>
 <div class="row"><span>Akkuspannung</span><span class="val" id="volt">-</span></div>
 <div class="row"><span>Distanz</span><span class="val" id="dist">-</span></div>
 <div class="row"><span>Letzter Kontakt</span><span class="val" id="seen">-</span></div>
 <button id="btnReset" onclick="doReset()">Sender-Flags zur&uuml;cksetzen</button>
 <p class="note">Der Befehl wird im n&auml;chsten Empfangsfenster des Senders
 zugestellt (Sender meldet sich st&uuml;ndlich).</p>
</div>

<div class="card">
 <h2 style="margin-top:0">Gateway</h2>
 <div class="row"><span>Temperatur</span><span class="val" id="temp">-</span></div>
 <div class="row"><span>Luftfeuchte</span><span class="val" id="hum">-</span></div>
 <div class="row"><span>Status</span><span class="val" id="status">-</span></div>
</div>

<div class="card">
 <h2 style="margin-top:0">Konfiguration</h2>
 <form onsubmit="saveConfig(event)">
  <label>WLAN SSID</label>
  <input name="wifi_ssid" id="wifi_ssid">
  <label>WLAN Passwort (leer = unver&auml;ndert)</label>
  <input name="wifi_pass" type="password" autocomplete="new-password">
  <label>SMTP Benutzer</label>
  <input name="smtp_user" id="smtp_user">
  <label>SMTP Passwort (leer = unver&auml;ndert)</label>
  <input name="smtp_pass" type="password" autocomplete="new-password">
  <label>Empf&auml;nger (kommagetrennt)</label>
  <input name="recipients" id="recipients">
  <button type="submit">Speichern und Neustart</button>
  <p class="note">Der AES-Schl&uuml;ssel ist aus Sicherheitsgr&uuml;nden nur
  &uuml;ber die .env Provisionierung &auml;nderbar.</p>
 </form>
</div>

<script>
function fmtSeen(s){
 if(s<0)return "noch kein Kontakt";
 if(s<120)return s+" s";
 if(s<7200)return Math.round(s/60)+" min";
 return (s/3600).toFixed(1)+" h";
}
async function poll(){
 try{
  const r=await fetch('/api/status');
  const d=await r.json();
  const mail=document.getElementById('mail');
  mail.textContent=d.valid?(d.mail?"POST EINGEWORFEN":"leer"):"-";
  mail.className="val "+(d.mail?"warn":"ok");
  const batt=document.getElementById('batt');
  batt.textContent=d.valid?(d.batt?"AKKU NIEDRIG":"in Ordnung"):"-";
  batt.className="val "+(d.batt?"err":"ok");
  document.getElementById('volt').textContent=d.valid?d.voltage.toFixed(2)+" V":"-";
  document.getElementById('dist').textContent=d.valid?d.distance+" mm":"-";
  document.getElementById('seen').textContent=fmtSeen(d.last_seen_s);
  document.getElementById('temp').textContent=d.climate_ok?d.temp.toFixed(1)+" \u00B0C":"-";
  document.getElementById('hum').textContent=d.climate_ok?d.hum.toFixed(1)+" %":"-";
  document.getElementById('status').textContent=d.status;
  const b=document.getElementById('btnReset');
  b.disabled=d.reset_pending;
  b.textContent=d.reset_pending?"Reset wartet auf Sender...":"Sender-Flags zur\u00FCcksetzen";
 }catch(e){}
}
async function doReset(){
 await fetch('/api/reset',{method:'POST'});
 poll();
}
async function saveConfig(ev){
 ev.preventDefault();
 const fd=new FormData(ev.target);
 const r=await fetch('/api/config',{method:'POST',body:new URLSearchParams(fd)});
 alert(await r.text());
}
async function loadConfig(){
 try{
  const r=await fetch('/api/config');
  const d=await r.json();
  document.getElementById('wifi_ssid').value=d.wifi_ssid;
  document.getElementById('smtp_user').value=d.smtp_user;
  document.getElementById('recipients').value=d.recipients;
 }catch(e){}
}
poll();loadConfig();setInterval(poll,5000);
</script>
</body>
</html>)HTML";

// ------------------------------------------------------------
// Hilfsfunktion: String JSON-sicher maskieren
// ------------------------------------------------------------
static String jsonEscape(const String& in) {
    String out;
    out.reserve(in.length() + 4);
    for (size_t i = 0; i < in.length(); i++) {
        char c = in.charAt(i);
        if (c == '"' || c == '\\') { out += '\\'; out += c; }
        else if (c == '\n' || c == '\r') { out += ' '; }
        else out += c;
    }
    return out;
}

bool WebPortal::begin(NvsConfig* cfg, const String& password) {
    cfg_  = cfg;
    pass_ = password;

    if (pass_.length() == 0) {
        Serial.println("Warnung: Portal ohne Passwortschutz, PORTAL_PASS in .env setzen");
    }

    server_.on("/", HTTP_GET, [this]() { handleRoot(); });
    server_.on("/api/status", HTTP_GET, [this]() { handleApiStatus(); });
    server_.on("/api/reset",  HTTP_POST, [this]() { handleApiReset(); });
    server_.on("/api/config", HTTP_GET,  [this]() { handleApiConfig(); });
    server_.on("/api/config", HTTP_POST, [this]() { handleApiConfig(); });
    server_.onNotFound([this]() { server_.send(404, "text/plain", "Nicht gefunden"); });
    server_.begin();

    // mDNS: Portal unter http://briefkastensensor.local/ erreichbar
    if (MDNS.begin(MDNS_HOSTNAME)) {
        MDNS.addService("http", "tcp", 80);
        Serial.print("Portal erreichbar unter http://");
        Serial.print(MDNS_HOSTNAME);
        Serial.print(".local/ oder http://");
        Serial.println(WiFi.localIP());
    } else {
        Serial.print("mDNS nicht verfuegbar, Portal unter http://");
        Serial.println(WiFi.localIP());
    }
    return true;
}

void WebPortal::handle() {
    server_.handleClient();

    // Verzoegerten Neustart nach Konfigurationsaenderung ausfuehren
    if (restart_at_ms_ != 0 && millis() > restart_at_ms_) {
        Serial.println("Neustart nach Konfigurationsaenderung");
        delay(100);
        ESP.restart();
    }
}

void WebPortal::updateRemote(uint8_t mail, uint8_t batt, float voltage, uint16_t distance) {
    remote_.valid        = true;
    remote_.mail_flag    = mail;
    remote_.batt_flag    = batt;
    remote_.voltage      = voltage;
    remote_.distance     = distance;
    remote_.last_seen_ms = millis();
}

// ------------------------------------------------------------
// HTTP Basic Auth, sofern ein Portal-Passwort hinterlegt ist
// ------------------------------------------------------------
bool WebPortal::requireAuth() {
    if (pass_.length() == 0) return true;
    if (!server_.authenticate("admin", pass_.c_str())) {
        server_.requestAuthentication();
        return false;
    }
    return true;
}

void WebPortal::handleRoot() {
    if (!requireAuth()) return;
    server_.send_P(200, "text/html", PAGE_HTML);
}

void WebPortal::handleApiStatus() {
    if (!requireAuth()) return;

    // Letzter Kontakt in Sekunden, -1 wenn noch nie
    long seen_s = -1;
    if (remote_.valid) {
        seen_s = (long)((millis() - remote_.last_seen_ms) / 1000UL);
    }

    float temp = 0, hum = 0;
    bool climate_ok = climate_ ? climate_(temp, hum) : false;

    String json = "{";
    json += "\"valid\":" + String(remote_.valid ? "true" : "false");
    json += ",\"mail\":" + String(remote_.mail_flag);
    json += ",\"batt\":" + String(remote_.batt_flag);
    json += ",\"voltage\":" + String(remote_.voltage, 2);
    json += ",\"distance\":" + String(remote_.distance);
    json += ",\"last_seen_s\":" + String(seen_s);
    json += ",\"reset_pending\":" + String(reset_pending_ ? "true" : "false");
    json += ",\"climate_ok\":" + String(climate_ok ? "true" : "false");
    json += ",\"temp\":" + String(temp, 1);
    json += ",\"hum\":" + String(hum, 1);
    json += ",\"status\":\"" + jsonEscape(status_text_ ? *status_text_ : String("")) + "\"";
    json += "}";
    server_.send(200, "application/json", json);
}

void WebPortal::handleApiReset() {
    if (!requireAuth()) return;
    reset_pending_ = true;
    Serial.println("Portal: Flag-Reset vorgemerkt");
    server_.send(200, "text/plain", "Reset vorgemerkt");
}

void WebPortal::handleApiConfig() {
    if (!requireAuth()) return;

    if (server_.method() == HTTP_GET) {
        // Nur unkritische Werte ausliefern, Passwoerter niemals anzeigen
        String json = "{";
        json += "\"wifi_ssid\":\"" + jsonEscape(cfg_->readString("wifi_ssid")) + "\"";
        json += ",\"smtp_user\":\"" + jsonEscape(cfg_->readString("smtp_user")) + "\"";
        json += ",\"recipients\":\"" + jsonEscape(cfg_->readString("recipients")) + "\"";
        json += "}";
        server_.send(200, "application/json", json);
        return;
    }

    // POST: Werte uebernehmen, leere Passwortfelder lassen den
    // gespeicherten Wert unveraendert
    bool changed = false;
    const char* keys[] = {"wifi_ssid", "smtp_user", "recipients"};
    for (const char* key : keys) {
        if (server_.hasArg(key)) {
            String val = server_.arg(key);
            val.trim();
            if (val.length() > 0 && val != cfg_->readString(key)) {
                changed |= cfg_->writeString(key, val);
            }
        }
    }
    const char* secrets[] = {"wifi_pass", "smtp_pass"};
    for (const char* key : secrets) {
        if (server_.hasArg(key)) {
            String val = server_.arg(key);
            if (val.length() > 0) {
                changed |= cfg_->writeString(key, val);
            }
        }
    }

    if (changed) {
        server_.send(200, "text/plain",
                     "Gespeichert. Das Gateway startet in wenigen Sekunden neu.");
        restart_at_ms_ = millis() + 2000;
    } else {
        server_.send(200, "text/plain", "Keine Aenderungen erkannt.");
    }
}
