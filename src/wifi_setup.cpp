#include "config.h"

#if BOARD_TYPE == BOARD_ESP01S
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>
#include <stddef.h>
#include "server.h"
#include "wifi_setup.h"

namespace {
constexpr uint32_t MAGIC = 0x57535531;
constexpr uint32_t CONNECT_TIMEOUT = 30000;
constexpr uint32_t AP_GRACE = 30000;
struct Credentials {
    uint32_t magic;
    char ssid[33];
    char password[65];
    uint32_t checksum;
};
Credentials saved = {}, candidate = {};
enum class State { Ap, Connecting, Connected, Grace };
State state = State::Ap;
uint32_t stateSince = 0;
bool candidatePending = false;
bool requestConnect = false;
bool requestScan = false;
bool hasSaved = false;
bool apActive = false;
#if WIFI_MDNS_ENABLE
bool mdnsStarted = false;
#endif
uint32_t connectRequestedAt = 0;
String result = "";

uint32_t checksum(const Credentials &c) {
    uint32_t hash = 2166136261UL;
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&c);
    for (size_t i = 0; i < offsetof(Credentials, checksum); ++i) hash = (hash ^ bytes[i]) * 16777619UL;
    return hash;
}

bool loadCredentials() {
    EEPROM.begin(sizeof(Credentials));
    EEPROM.get(0, saved);
    EEPROM.end();
    return saved.magic == MAGIC && saved.ssid[0] && saved.ssid[32] == '\0' &&
           saved.password[64] == '\0' && saved.checksum == checksum(saved);
}

bool saveCredentials() {
    candidate.magic = MAGIC;
    candidate.checksum = checksum(candidate);
    EEPROM.begin(sizeof(Credentials));
    EEPROM.put(0, candidate);
    bool ok = EEPROM.commit();
    EEPROM.end();
    if (ok) saved = candidate;
    return ok;
}

void startAp() {
    WiFi.mode(WIFI_AP_STA);
    IPAddress address(192, 168, 200, 200), mask(255, 255, 255, 0);
    WiFi.softAPConfig(address, address, mask);
    WiFi.softAP("WiFi-UART");
    apActive = true;
    state = State::Ap;
    stateSince = millis();
}

void connect(const Credentials &c, bool keepAp) {
    WiFi.disconnect();
    WiFi.mode(keepAp ? WIFI_AP_STA : WIFI_STA);
    WiFi.hostname(WIFI_HOSTNAME);
    WiFi.begin(c.ssid, c.password);
    state = State::Connecting;
    stateSince = millis();
}

const char *stateName() {
    switch (state) {
        case State::Connecting: return "connecting";
        case State::Connected: return "connected";
        case State::Grace: return "connected";
        default: return "ap";
    }
}

const char page[] PROGMEM = R"PAGE(<!doctype html><html lang="pl"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>WiFi-UART</title>
<style>body{font:16px sans-serif;background:#222;color:#eee;max-width:38em;margin:2em auto;padding:0 1em}input,select,button{font:inherit;padding:.5em;margin:.4em 0;max-width:100%}label{display:block}a{color:#9cf}#status{white-space:pre-wrap}</style>
<h1>Wi-Fi</h1><p><a href="/">Terminal UART</a></p><button id="scan" type="button">Skanuj sieci</button><label>Sieć <select id="networks"><option value="">Wybierz sieć</option></select></label><label>Nazwa sieci (SSID) <input id="ssid" maxlength="32" required></label><label>Hasło <input id="password" type="password" maxlength="64" autocomplete="off"></label><button id="connect" type="button">Zapisz i połącz</button><p id="status"></p>
<script>
const $=id=>document.getElementById(id), status=$('status');
async function update(){try{let r=await fetch('/wifi/status',{cache:'no-store'});if(!r.ok)throw Error();let s=await r.json();status.textContent='Stan: '+s.state+(s.ip?'\nAdres STA: http://'+s.ip+'/':'')+(s.result?'\n'+s.result:'');}catch(e){status.textContent='Brak połączenia. Połącz się z WiFi-UART lub sprawdź adres urządzenia w routerze.'}}
$('scan').onclick=async()=>{try{let r=await fetch('/wifi/scan',{method:'POST',headers:{'X-WiFi-Setup':'1'}});if(!r.ok)throw Error();status.textContent='Skanowanie...';setTimeout(results,3000)}catch(e){status.textContent='Nie można rozpocząć skanowania.'}};
async function results(){try{let r=await fetch('/wifi/scan');let d=await r.json();if(d.scanning){setTimeout(results,1500);return}let list=$('networks');list.replaceChildren(new Option('Wybierz sieć',''));d.networks.forEach(n=>list.add(new Option(n.ssid+' ('+n.rssi+' dBm)',n.ssid)));status.textContent='Znaleziono sieci: '+d.networks.length}catch(e){status.textContent='Nie udało się pobrać sieci.'}}
$('networks').onchange=()=>{$('ssid').value=$('networks').value};
$('connect').onclick=async()=>{try{let body=new URLSearchParams({ssid:$('ssid').value,password:$('password').value});let r=await fetch('/wifi/connect',{method:'POST',headers:{'X-WiFi-Setup':'1'},body});let t=await r.text();if(!r.ok)throw Error(t);$('password').value='';status.textContent='Trwa łączenie. Połączenie z AP może zostać przerwane. Jeżeli konfigurujesz przez STA, połącz się z WiFi-UART i otwórz 192.168.200.200/wifi.'}catch(e){status.textContent='Błąd: '+e.message}};
update();setInterval(update,3000);
</script></html>)PAGE";
}

void wifiSetupBegin() {
    WiFi.persistent(false);
    WiFi.setAutoConnect(false);
    WiFi.setAutoReconnect(false);
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
    hasSaved = loadCredentials();
    if (hasSaved) connect(saved, false);
    else startAp();
}

void wifiSetupLoop() {
    if (requestScan) {
        requestScan = false;
        WiFi.scanDelete();
        WiFi.scanNetworks(true);
    }
    if (requestConnect && millis() - connectRequestedAt >= 500) {
        requestConnect = false;
        candidatePending = true;
        result = "Próba połączenia z wybraną siecią.";
        if (!apActive) startAp();
        connect(candidate, true);
    }
    if (state == State::Connecting) {
        if (WiFi.status() == WL_CONNECTED) {
            if (candidatePending) {
                bool stored = saveCredentials();
                if (stored) hasSaved = true;
                result = stored ? "Połączono i zapisano sieć. AP zostanie wyłączony za 30 sekund." : "Połączono, ale zapis konfiguracji nie powiódł się.";
                candidatePending = false;
                state = State::Grace;
            } else {
                result = "Połączono z zapisaną siecią.";
                state = apActive ? State::Grace : State::Connected;
            }
            stateSince = millis();
#if WIFI_MDNS_ENABLE
            if (mdnsStarted) MDNS.close();
            mdnsStarted = MDNS.begin(WIFI_HOSTNAME);
            if (mdnsStarted) MDNS.addService("http", "tcp", HTTP_LISTEN_PORT);
#endif
        } else if (millis() - stateSince >= CONNECT_TIMEOUT) {
            WiFi.disconnect();
            candidatePending = false;
            result = "Nie udało się połączyć. Poprzednia konfiguracja pozostała bez zmian.";
            startAp();
        }
    } else if (state == State::Grace && millis() - stateSince >= AP_GRACE) {
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
        apActive = false;
        state = State::Connected;
    } else if ((state == State::Connected || state == State::Grace) && WiFi.status() != WL_CONNECTED) {
        if (hasSaved) connect(saved, apActive);
        else startAp();
    }
#if WIFI_MDNS_ENABLE
    if (mdnsStarted) MDNS.update();
#endif
}

void wifiSetupRoutes(AsyncWebServer *server) {
    server->on("/wifi", HTTP_GET, [](AsyncWebServerRequest *req) {
        if (!WiSeServer::checkHttpBasicAuth(req)) return;
        req->send_P(200, "text/html; charset=utf-8", page);
    });
    server->on("/wifi/status", HTTP_GET, [](AsyncWebServerRequest *req) {
        if (!WiSeServer::checkHttpBasicAuth(req)) return;
        AsyncResponseStream *res = req->beginResponseStream("application/json");
        DynamicJsonDocument doc(512);
        doc["state"] = stateName();
        doc["result"] = result;
        doc["ip"] = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "";
        serializeJson(doc, *res);
        req->send(res);
    });
    server->on("/wifi/scan", HTTP_POST, [](AsyncWebServerRequest *req) {
        if (!WiSeServer::checkHttpBasicAuth(req)) return;
        if (!req->hasHeader("X-WiFi-Setup")) { req->send(403); return; }
        if (state == State::Connecting || requestScan) { req->send(409, "text/plain", "Zajęte"); return; }
        requestScan = true;
        req->send(202, "text/plain", "OK");
    });
    server->on("/wifi/scan", HTTP_GET, [](AsyncWebServerRequest *req) {
        if (!WiSeServer::checkHttpBasicAuth(req)) return;
        int count = WiFi.scanComplete();
        AsyncResponseStream *res = req->beginResponseStream("application/json");
        res->print("{\"scanning\":");
        res->print((count == WIFI_SCAN_RUNNING || requestScan) ? "true" : "false");
        res->print(",\"networks\":[");
        for (int i = 0, emitted = 0; i < count && emitted < 30; ++i) {
            String ssid = WiFi.SSID(i);
            if (!ssid.length()) continue;
            if (emitted++) res->print(',');
            StaticJsonDocument<160> item;
            item["ssid"] = ssid;
            item["rssi"] = WiFi.RSSI(i);
            serializeJson(item, *res);
        }
        res->print("]}");
        req->send(res);
    });
    server->on("/wifi/connect", HTTP_POST, [](AsyncWebServerRequest *req) {
        if (!WiSeServer::checkHttpBasicAuth(req)) return;
        if (!req->hasHeader("X-WiFi-Setup")) { req->send(403); return; }
        if (state == State::Connecting || requestConnect || requestScan || WiFi.scanComplete() == WIFI_SCAN_RUNNING) { req->send(409, "text/plain", "Urządzenie jest zajęte"); return; }
        if (!req->hasParam("ssid", true) || !req->hasParam("password", true)) { req->send(400, "text/plain", "Brak danych"); return; }
        String ssid = req->getParam("ssid", true)->value();
        String password = req->getParam("password", true)->value();
        if (!ssid.length() || ssid.length() > 32 || password.length() > 64 || (password.length() > 0 && password.length() < 8)) {
            req->send(400, "text/plain", "Nieprawidłowa nazwa lub hasło"); return;
        }
        candidate = {};
        ssid.toCharArray(candidate.ssid, sizeof(candidate.ssid));
        password.toCharArray(candidate.password, sizeof(candidate.password));
        connectRequestedAt = millis();
        requestConnect = true;
        req->send(202, "text/plain", "OK");
    });
}
#endif
