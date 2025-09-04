/***************************************************************************
 * ESP32 Multi-Function Web Server
 *
 * This code provides:
 *  - Sensor data collection from DS18B20 sensors.
 *  - A web interface for:
 *       * Displaying sensor data (Main page).
 *       * Managing OTA updates.
 *       * Managing sensor labels.
 *       * Filesystem management (list, view, download, delete, upload, format, and FS info).
 *       * Connectivity management: manually disconnect and reconnect to WiFi.
 *
 * All HTML pages are stored in flash (using PROGMEM) to conserve RAM.
 * Detailed inline comments explain each section.
 ***************************************************************************/

 #define FIRMWARE_VERSION "1.1.2"

 //---------------------------------------------------------------------
 // LIBRARY INCLUDES
 //---------------------------------------------------------------------
 #include <WiFi.h>
 #include <ESPmDNS.h>
 #include <Update.h>
 #include <OneWire.h>
 #include <DallasTemperature.h>
 #include <LittleFS.h>
 #include <HTTPClient.h>
 #include <ArduinoJson.h>
 #include <ArduinoOTA.h>
 #include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <AsyncMqttClient.h>
#include <DHT.h>
#define DHTPIN 14
#define DHTTYPE DHT22


// ---- DHT read throttle/cache (avoid oversampling) ----
static uint32_t lastDhtRead = 0;
static float    lastHum = NAN, lastTc = NAN;
const  uint32_t DHT_MIN_INTERVAL_MS = 2000; // DHT22 needs ≥2s between reads

// ---- Alert thresholds (runtime-configurable; persisted in /config.json) ----
// Temperature (°F)
float gTEMP_F_WARN_HIGH = 85.0;
float gTEMP_F_CRIT_HIGH = 95.0;
float gTEMP_F_WARN_LOW  = 32.0;
float gTEMP_F_CRIT_LOW  = 20.0;

// Humidity (% RH)
float gHUM_WARN_HIGH = 70.0;
float gHUM_CRIT_HIGH = 80.0;
float gHUM_WARN_LOW  = 25.0;
float gHUM_CRIT_LOW  = 15.0;

// Filesystem usage (%)
uint8_t gFS_WARN_PCT  = 80;
uint8_t gFS_CRIT_PCT  = 95;

// Connectivity (MQTT heartbeat age)
uint32_t gMQTT_WARN_MS = 60UL * 1000UL;        // 60s
uint32_t gMQTT_CRIT_MS = 5UL  * 60UL * 1000UL; // 5 min

// Active profile
String gProfile = "default";

// --- Temporary backward-compat (so current code still compiles) ---
#define TEMP_F_HIGH        gTEMP_F_WARN_HIGH
#define TEMP_F_LOW         gTEMP_F_WARN_LOW
#define HUMIDITY_HIGH      gHUM_WARN_HIGH
#define HUMIDITY_LOW       gHUM_WARN_LOW
#define FS_USED_WARN_PCT   gFS_WARN_PCT


// ================= MQTT CONFIG (EDIT THESE) =================
static const char* MQTT_HOST = "test.mosquitto.org";
static const uint16_t MQTT_PORT = 1883;          
static const char* MQTT_USER = "";               
static const char* MQTT_PASS = "";               

// Intervals
static const uint32_t HEARTBEAT_MS = 30 * 1000;
static const uint32_t HEALTH_MS    = 15 * 1000;
static const uint32_t TELEMETRY_MS = 5 * 1000;
static const uint32_t ALERTS_MS    = 5 * 1000;

// ================= MQTT STATE =================
AsyncMqttClient mqtt;
String gClientId;
String mqttBaseTopic;
unsigned long lastMqttHeartbeat = 0;
unsigned long lastMqttHealth    = 0;
unsigned long lastMqttTelemetry = 0;
unsigned long lastMqttAlerts    = 0;
String deviceIdNoColon() {
  String mac = WiFi.macAddress();
  String out; out.reserve(mac.length());
  for (char c: mac) if (c != ':') out += c;
  return out;
}

String topicBase() {
  if (mqttBaseTopic.length() == 0) {
    mqttBaseTopic = String("dev/") + deviceIdNoColon();
  }
  return mqttBaseTopic;
}

// Build heartbeat JSON
void buildHeartbeatDoc(JsonDocument &doc, const char* status = "ONLINE") {
  String ipAddress = (WiFi.getMode() == WIFI_AP) ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  doc["deviceId"] = deviceIdNoColon();
  doc["fw"]       = FIRMWARE_VERSION;
  doc["ip"]       = ipAddress;
  doc["rssi"]     = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : -127;
  doc["uptimeMs"] = (uint32_t)millis();
  doc["ts"]       = (uint32_t)millis();
  doc["status"]   = status;
}

// Build health JSON
void buildHealthDoc(JsonDocument &doc) {
  doc["deviceId"] = deviceIdNoColon();
  doc["ts"]       = (uint32_t)millis();
  doc["rssi"]     = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : -127;

  JsonObject h = doc["health"].to<JsonObject>();
  uint32_t total = LittleFS.totalBytes();
  uint32_t used  = LittleFS.usedBytes();
  h["freeHeap"]    = (uint32_t)ESP.getFreeHeap();
  h["minFreeHeap"] = (uint32_t)ESP.getMinFreeHeap();
  h["fsUsedPct"]   = total ? (uint32_t)((100.0 * used) / total) : 0;
  h["fsFree"]      = (total > used) ? (uint32_t)(total - used) : 0;
  h["queueBytes"]  = 0; 
}

// Publish helpers
bool mqttIsConnected() { return mqtt.connected(); }

bool mqttPublish(const String& subPath, const JsonDocument& doc, bool retain=false, uint8_t qos=1) {
  String topic = topicBase() + subPath;
  String payload; serializeJson(doc, payload);
  return mqtt.publish(topic.c_str(), qos, retain, payload.c_str());
}

bool mqttPublishRaw(const String& subPath, const char* payload, bool retain=false, uint8_t qos=1) {
  String topic = topicBase() + subPath;
  return mqtt.publish(topic.c_str(), qos, retain, payload);
}

void publishTelemetryAndAlerts(const JsonDocument &doc) {
  if (!mqttIsConnected()) return;

  const unsigned long now = millis();

  // ---- Telemetry (devices/<MAC>/telemetry) ----
  if (now - lastMqttTelemetry >= TELEMETRY_MS) {
    JsonDocument telem;
    telem["deviceId"] = deviceIdNoColon();
    telem["ts"]       = (uint32_t)now;
    // deep-copy sensor array
    if (doc["sensors"].is<JsonArray>()) {
      telem["sensors"] = doc["sensors"];
    }
    mqttPublish("/telemetry", telem, /*retain=*/false, /*qos=*/0);
    lastMqttTelemetry = now;
  }

  // ---- Critical alerts (dev/<MAC>/critical) ----
  if (doc["alerts"].is<JsonArray>() && doc["alerts"].size() > 0) {
    if (now - lastMqttAlerts >= ALERTS_MS) {
      JsonDocument alertsDoc;
      alertsDoc["deviceId"] = deviceIdNoColon();
      alertsDoc["ts"]       = (uint32_t)now;
      alertsDoc["alerts"]   = doc["alerts"]; 
      mqttPublish("/critical", alertsDoc, /*retain=*/false, /*qos=*/1);
      lastMqttAlerts = now;
    }
  }
}

void mqttConnect() {
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  if (strlen(MQTT_USER)) mqtt.setCredentials(MQTT_USER, MQTT_PASS);
  gClientId = String("esp32-") + deviceIdNoColon();
  mqtt.setClientId(gClientId.c_str());
  // LWT: if device drops unexpectedly, broker publishes offline to /critical
  String willTopic = topicBase() + "/critical";
  mqtt.setWill(willTopic.c_str(), 1, true, "offline");

  mqtt.onConnect([](bool){
    // Presence: publish retained "online" to /critical
    mqttPublishRaw("/critical", "online", true, 1);
    // Optional: also send a retained heartbeat snapshot for dashboards
    JsonDocument hb;
    buildHeartbeatDoc(hb, "ONLINE");
    mqttPublish("/heartbeat", hb, true, 1);
  });

  mqtt.onDisconnect([](AsyncMqttClientDisconnectReason){
    // Auto-reconnect after a short delay
    static unsigned long nextTry = 0;
    if (millis() > nextTry) { nextTry = millis() + 2000; mqtt.connect(); }
  });

  mqtt.connect();
}

String processor(const String& var) {
  if (var == "firmwareVersion") {
    return FIRMWARE_VERSION;
  }
  return String();
}
 
 //---------------------------------------------------------------------
 // DYNAMIC HTML PAGES (Stored in flash)
 //---------------------------------------------------------------------
// Main page: Displays sensor readings, IP address, and status.
const char webpage[] PROGMEM = R"rawliteral(
 <!DOCTYPE html>
 <html>
 <head>
   <title>WinWinLabs Real-Time Data</title>
   <style>
     body { font-family: Arial, sans-serif; text-align: center; background: #f4f4f4; margin: 0; }
     header { background:#1f2937; color:#fff; padding:12px 16px; text-align:left; }
     h1 { color: #333; margin-top: 10px; }
     .wrap { max-width: 900px; margin: 0 auto; padding: 12px; }
     .data { font-size: 1.1em; margin: 8px; }
     .sensor { margin: 10px 0; padding: 10px; background: #fff; border-radius: 5px;
               box-shadow: 0 0 5px rgba(0,0,0,0.1); text-align:left; }
     .footer { margin: 20px 0; font-size: 0.9em; color: #777; }
     .button { background-color: #3498db; color: white; padding: 10px 20px;
               text-decoration: none; border-radius: 5px; display: inline-block; }
     .button:hover { background-color: #2377a5; }

     /* ESP status pills */
     .pills { display:flex; gap:8px; flex-wrap:wrap; justify-content:center; margin:8px 0; }
     .pill { padding:6px 10px; border-radius:999px; font-size:0.9em; background:#e5e7eb; color:#111; }
     .ok   { background:#d1fae5; }
     .warn { background:#fde68a; }
     .err  { background:#fecaca; }

     /* Alerts list */
     #alerts { max-width:900px; margin:10px auto; text-align:left; }
     .alert { padding:10px; border-radius:6px; margin:8px 0; }
     .alert.info  { background:#e0f2fe; }
     .alert.warn  { background:#fff7ed; }
     .alert.error { background:#fee2e2; }
     .kv { display:grid; grid-template-columns: 160px 1fr; gap:6px 12px; margin:10px auto; max-width:600px; text-align:left; }
     .kv b { color:#374151; }
   </style>
 </head>
 <body>
   <header><strong>WWL Device Dashboard</strong></header>
   <div class="wrap">
     <h1>Winwinlab Temp &amp; Humidity</h1>

     <div class="data">Firmware Version: <span id="firmwareVersion">%firmwareVersion%</span></div>

     <div class="pills" id="espPills">
       <!-- Pills populated by JS -->
     </div>

     <div class="kv">
       <b>IP Address:</b> <span id="ipAddress">--</span>
       <b>Status:</b>     <span id="status">--</span>
       <b>Uptime:</b>     <span id="uptime">--</span>
       <b>WiFi RSSI:</b>  <span id="rssi">--</span>
       <b>FS Usage:</b>   <span id="fs">--</span>
     </div>

     <div id="alerts"></div>

     <h2>Live Sensors</h2>
     <div id="sensors"></div>

     <div class="footer">
       <a href="/manage" class="button">Manage</a>
       <p>Updates every ~0.5–1s</p>
     </div>
   </div>
   <script>
    // put the templated firmware version into the span right away
    document.addEventListener('DOMContentLoaded', () => {
      document.getElementById('firmwareVersion').textContent = '%firmwareVersion%';
    });

    function fmtUptime(ms){
      const s=Math.floor(ms/1000), d=Math.floor(s/86400), h=Math.floor((s%86400)/3600),
            m=Math.floor((s%3600)/60), ss=s%60;
      return (d?d+'d ':'')+h+'h '+m+'m '+ss+'s';
    }
    function pill(label, cls){ return `<span class="pill ${cls}">${label}</span>`; }

    function updateUI(data){
      document.getElementById("ipAddress").textContent = data.ipAddress || "--";
      document.getElementById("status").textContent    = data.status || "--";

      const esp = data.esp || {};
      document.getElementById("uptime").textContent = esp.uptimeMs ? fmtUptime(esp.uptimeMs) : "--";
      document.getElementById("rssi").textContent   = (esp.wifi &amp;&amp; (esp.wifi.rssi!==undefined)) ? esp.wifi.rssi + " dBm" : "--";
      document.getElementById("fs").textContent     = (esp.fs ? `${esp.fs.used}/${esp.fs.total} (${esp.fs.usedPct||0}%)` : "--");

      const pills = [];
      pills.push(pill(`WiFi: ${esp.wifi &amp;&amp; esp.wifi.connected ? 'Connected' : 'Offline'}`, esp.wifi &amp;&amp; esp.wifi.connected ? 'ok':'err'));
      pills.push(pill(`MQTT: ${esp.mqttConnected ? 'Connected' : 'Offline'}`, esp.mqttConnected ? 'ok' : 'warn'));
      pills.push(pill(`Capture: ${data.status==='Paused' ? 'Paused' : 'Running'}`, data.status==='Paused' ? 'warn':'ok'));
      document.getElementById("espPills").innerHTML = pills.join('');

      const alertsDiv = document.getElementById("alerts");
      alertsDiv.innerHTML = "";
      if (Array.isArray(data.alerts)) {
        data.alerts.forEach(a => {
          const div = document.createElement('div');
          div.className = `alert ${a.severity||'info'}`;
          div.textContent = a.message || '';
          alertsDiv.appendChild(div);
        });
      }

      const sensorsDiv = document.getElementById("sensors");
      sensorsDiv.innerHTML = "";
      if (data.sensors) {
        data.sensors.forEach(sensor => {
          const sensorDiv = document.createElement("div");
          sensorDiv.className = "sensor";
          sensorDiv.innerHTML = `${sensor.label} &nbsp;&nbsp; ${sensor.reading_value}`;
          sensorsDiv.appendChild(sensorDiv);
        });
      }
    }

    let ws, pollTimer;
    function startWS(){
      ws = new WebSocket(`ws://${location.host}/ws`);
      ws.onopen = () => { console.log("WebSocket open"); if (pollTimer) { clearInterval(pollTimer); pollTimer=null; } };
      ws.onmessage = (e) => { try { updateUI(JSON.parse(e.data)); } catch(_){} };
      ws.onerror = startPolling;
      ws.onclose = startPolling;
    }
    function startPolling(){
      if (pollTimer) return;
      console.log('WS unavailable, fallback to HTTP polling');
      pollTimer = setInterval(async ()=>{
        try{
          // Pull status and sensors concurrently, then merge
          const [stResp, sensResp] = await Promise.all([
            fetch('/status'),
            fetch('/get-sensors')
          ]);
          const st   = await stResp.json();
          const sens = await sensResp.json();
          st.sensors = sens.sensors || [];
          updateUI(st);
        }catch(e){
          console.log('poll error', e);
        }
      }, 1000);
    }
    document.addEventListener('DOMContentLoaded', startWS);
  </script>
 </body>
 </html>
)rawliteral";
 
 // Manage page: Provides navigation to OTA Update, Filesystem, Labels, and Connectivity pages.
 const char managePage[] PROGMEM = R"rawliteral(
 <!DOCTYPE html>
 <html>
 <head>
   <title>Manage</title>
   <style>
     body { font-family: Arial, sans-serif; text-align: center; background: #f4f4f4; }
     h1 { color: #333; }
     .button { background-color: #3498db; color: white; padding: 10px 20px;
               text-decoration: none; border-radius: 5px; display: inline-block; margin: 5px; }
     .button:hover { background-color: #2377a5; }
   </style>
 </head>
 <body>
   <h1>Manage</h1>
   <div>
     <a href="/login" class="button">OTA Update</a>
     <a href="/fs" class="button">Filesystem</a>
     <a href="/labels" class="button">Labels</a>
     <a href="/connectivity" class="button">Connectivity</a>
     <a href="/" class="button">Back</a>
     <button id="toggleCapture" class="button" onclick="toggleCapture()">Pause Data Collection</button>
     <button class="button" onclick="rebootDevice()">Reboot Device</button>
   </div>
   <script>
     function toggleCapture() {
       const button = document.getElementById('toggleCapture');
       if (button.textContent === 'Pause Data Collection') {
         fetch('/stop-capture', { method: 'POST' })
           .then(response => response.text())
           .then(data => {
             console.log(data);
             button.textContent = 'Start Data Collection';
           });
       } else {
         fetch('/start-capture', { method: 'POST' })
           .then(response => response.text())
           .then(data => {
             console.log(data);
             button.textContent = 'Pause Data Collection';
           });
       }
     }
 
     function rebootDevice() {
       fetch('/restart', { method: 'POST' })
         .then(response => response.text())
         .then(data => {
           console.log(data);
         });
     }
   </script>
 </body>
 </html>
 )rawliteral";
 
 // Labels Management page: Allows updating sensor labels.
 const char labelsPage[] PROGMEM = R"rawliteral(
 <!DOCTYPE html>
 <html>
 <head>
   <title>Labels Management</title>
   <style>
     body { font-family: Arial, sans-serif; text-align: center; background: #f4f4f4; }
     h1 { color: #333; }
     .sensor { margin: 10px 0; padding: 10px; background: #fff; border-radius: 5px;
               box-shadow: 0 0 5px rgba(0,0,0,0.1); }
     .button { background-color: #3498db; color: white; padding: 10px 20px;
               text-decoration: none; border-radius: 5px; display: inline-block; margin: 5px; }
     .button:hover { background-color: #2377a5; }
     form { display: inline-block; margin-top: 20px; }
   </style>
 </head>
 <body>
   <h1>Labels Management</h1>
   <div id="sensors"></div>
   <form id="labelInputs">
     <h2>Update Sensor Labels</h2>
     <div id="sensorInputs"></div>
     <button type="submit" class="button">Save Labels</button>
   </form>
   <br>
   <a href="/manage" class="button">Back</a>
   <script>
     let ws = new WebSocket(`ws://${location.host}/ws`);
     let wsPaused = false;
     ws.onmessage = function(event) {
       if (wsPaused) return;
       try {
         let data = JSON.parse(event.data);
         const sensorsDiv = document.getElementById("sensors");
         const sensorInputs = document.getElementById("sensorInputs");
         sensorsDiv.innerHTML = "";
         sensorInputs.innerHTML = "";
         if (data.sensors) {
           data.sensors.forEach(sensor => {
             const sensorDiv = document.createElement("div");
             sensorDiv.className = "sensor";
             sensorDiv.innerHTML = `${sensor.label}      ${sensor.reading_value}`;
             sensorsDiv.appendChild(sensorDiv);
             const inputDiv = document.createElement("div");
             inputDiv.innerHTML = `
               <label for="${sensor.id}">Label for ${sensor.id}:</label>
               <input type="text" id="${sensor.id}" name="${sensor.id}" value="${sensor.label.replace(/^S\\d+:\\s*/, '')}"
               onfocus="wsPaused = true;" onblur="wsPaused = false;">
             `;
             sensorInputs.appendChild(inputDiv);
           });
         }
       } catch(e) {
         console.error("Error parsing JSON:", e);
       }
     };
     document.getElementById("labelInputs").addEventListener("submit", function(e) {
       e.preventDefault();
       const labels = {};
       Array.from(document.getElementById("sensorInputs").children).forEach(div => {
         const input = div.querySelector("input");
         if (input) labels[input.id] = input.value;
       });
       fetch("/update-labels", {
         method: "POST",
         headers: { "Content-Type": "application/json" },
         body: JSON.stringify(labels)
       })
       .then(response => response.json())
       .then(data => {
         if (data.status === "success") {
           console.log("Labels updated successfully");
         } else {
           console.error("Failed to update labels:", data.error);
         }
       })
       .catch(err => console.error("Error updating labels:", err));
     });
   </script>
 </body>
 </html>
 )rawliteral";
 
 // Connectivity Management page: Lets user disconnect/reconnect WiFi and set new credentials.
 const char connectivityPage[] PROGMEM = R"rawliteral(
 <!DOCTYPE html>
 <html>
 <head>
   <title>Connectivity Management</title>
   <style>
     body { font-family: Arial, sans-serif; text-align: center; background: #f4f4f4; }
     h1 { color: #333; }
     .button { background-color: #3498db; color: white; padding: 10px 20px; text-decoration: none;
               border-radius: 5px; display: inline-block; margin: 5px; }
     .button:hover { background-color: #2377a5; }
     input[type="text"], input[type="password"] { padding: 8px; margin: 5px; }
   </style>
 </head>
 <body>
   <h1>Connectivity Management</h1>
   <div>
     <button onclick="disconnectWifi()" class="button">Disconnect WiFi (Force AP Mode)</button>
   </div>
   <div>
     <h2>Connect to a WiFi Network</h2>
     <button onclick="scanNetworks()" class="button">Scan for Networks</button>
     <br>
     <select id="networkList"></select>
     <br>
     <input type="text" id="ssid" placeholder="SSID">
     <input type="password" id="password" placeholder="Password">
     <br>
     <button onclick="connectWifi()" class="button">Connect</button>
   </div>
   <br>
   <a href="/manage" class="button">Back</a>
   <script>
     function disconnectWifi() {
    fetch("/disconnect", { method: "POST" })
       .then(response => response.text())
       .then(data => { alert(data); window.location.href = "/manage"; })
       .catch(err => console.error("Error disconnecting:", err));
     }
     function scanNetworks() {
       fetch("/scan")
       .then(response => response.json())
       .then(data => {
          let list = document.getElementById("networkList");
          list.innerHTML = "";
          data.networks.forEach(network => {
            let option = document.createElement("option");
            option.value = network.ssid;
            option.text = network.ssid + " (" + network.rssi + " dBm)";
            list.appendChild(option);
          });
          list.onchange = function() {
            document.getElementById("ssid").value = this.value;
          };
       })
       .catch(err => console.error("Error scanning networks:", err));
     }
     function connectWifi() {
       let ssid = document.getElementById("ssid").value;
       let password = document.getElementById("password").value;
       fetch("/connectivity", {
         method: "POST",
         headers: {"Content-Type": "application/json"},
         body: JSON.stringify({ ssid: ssid, password: password })
       })
       .then(response => response.text())
       .then(data => { alert(data); window.location.href = "/manage"; })
       .catch(err => console.error("Error connecting:", err));
     }
   </script>
 </body>
 </html>
 )rawliteral";
 
 // Filesystem Management page: Displays file list, FS info, and provides Download, View, Delete, and Upload.
 const char fsPage[] PROGMEM = R"rawliteral(
 <!DOCTYPE html>
 <html>
 <head>
   <title>Filesystem Management</title>
   <style>
     body { font-family: Arial, sans-serif; text-align: center; background: #f4f4f4; }
     h1 { color: #333; }
     table { margin: 0 auto; border-collapse: collapse; }
     th, td { padding: 8px 12px; border: 1px solid #ccc; }
     .button { background-color: #3498db; color: white; padding: 10px 20px; text-decoration: none;
               border-radius: 5px; display: inline-block; margin: 5px; }
     .button:hover { background-color: #2377a5; }
   </style>
 </head>
 <body>
   <h1>Filesystem Management</h1>
   <div id="fsInfo"></div>
   <div>
     <button onclick="listFiles()" class="button">Refresh File List</button>
     <a href="/format-fs" class="button">Format Filesystem</a>
     <a href="/manage" class="button">Back</a>
   </div>
   <div id="fileList">
     <!-- File list will be loaded here -->
   </div>
   <h2>Upload File</h2>
   <form method="POST" action="/upload-file" enctype="multipart/form-data">
     <input type="file" name="file">
     <input type="submit" value="Upload File" class="button">
   </form>
   <script>
     function listFiles() {
       fetch("/list-files")
       .then(response => response.json())
       .then(data => {
         let html = "<table><tr><th>Filename</th><th>Size (bytes)</th><th>Download</th><th>View</th><th>Delete</th></tr>";
         data.files.forEach(file => {
           html += "<tr>";
           html += "<td>" + file.name + "</td>";
           html += "<td>" + file.size + "</td>";
           html += "<td><a href='/download?file=" + encodeURIComponent(file.name) + "' class='button'>Download</a></td>";
           html += "<td><a href='/view-file?file=" + encodeURIComponent(file.name) + "' class='button' target='_blank'>View</a></td>";
           html += "<td><a href='/delete-file?file=" + encodeURIComponent(file.name) + "' class='button'>Delete</a></td>";
           html += "</tr>";
         });
         html += "</table>";
         document.getElementById("fileList").innerHTML = html;
       })
       .catch(err => console.error("Error fetching file list:", err));
     }
     function updateFsInfo() {
       fetch("/fsinfo")
       .then(response => response.json())
       .then(data => {
          document.getElementById("fsInfo").innerHTML =
             "Filesystem Total: " + data.total + " bytes, " +
             "Used: " + data.used + " bytes, " +
             "Free: " + data.free + " bytes";
       })
       .catch(err => console.error("Error fetching FS info:", err));
     }
     window.onload = function() {
       listFiles();
       updateFsInfo();
     };
   </script>
 </body>
 </html>
 )rawliteral";
 
 //---------------------------------------------------------------------
 // GLOBAL WIFI SETTINGS
 //---------------------------------------------------------------------
 const char* hostName = "esp32";
 String wifiSSID = "2.4guest";
 String wifiPassword = "11961Amherst";
 const char* remoteServerName = "http://ecoforces.com/update_db.php";
 String remoteApiKey = "tPmAT5Ab3j7F9";
 
 //---------------------------------------------------------------------
 // GLOBAL OBJECTS
 //---------------------------------------------------------------------
//WebServer server(83);
 //WebSocketsServer webSocket(81);
AsyncWebServer       server(83);
 AsyncWebSocket       ws("/ws");

 OneWire oneWire(4);
 DallasTemperature sensors(&oneWire);
 DHT dht(DHTPIN, DHTTYPE); 
 bool isAPMode = false;
 File fsUploadFile;
 unsigned long lastWebSocketTime = 0;
 unsigned long webSocketInterval = 500;
 unsigned long lastDBUpdateTime = 0;
 unsigned long dbUpdateInterval = 20000;
 bool captureEnabled = true; // Default: sensor data is captured.
 
 // Global variables for OTA progress.
 unsigned long bytesWritten = 0;
 unsigned long totalSize = 0;
 volatile int uploadProgress = 0;  
 volatile bool updateFinished = false;
 volatile bool updateSuccessful = false;
 
//---------------------------------------------------------------------
// FUNCTION PROTOTYPES
//---------------------------------------------------------------------
void initFileSystem();
void loadSensorLabels(JsonDocument &doc);
void saveSensorLabels(JsonDocument &doc);
void uploadStoredData();
void startAccessPoint();
void handleWebSocketMessage(uint8_t num, uint8_t *payload, size_t length);
void onWebSocketEvent(AsyncWebSocket*server,AsyncWebSocketClient *client, AwsEventType type, void *arg,uint8_t *data, size_t len);
void updateWebSocketClients();
void setupOTA();
void connectToWiFi();
void formatFileSystem();
void sendSensorData();
void storeLabelsLocally(JsonDocument &doc);
void storeDataLocally();
void handleFileUpload(AsyncWebServerRequest *req, const String &filename,size_t index, uint8_t *data,size_t len, bool final);
void serverFileListHandler(AsyncWebServerRequest *req);
void downloadFileHandler(AsyncWebServerRequest *req);
void viewFileHandler(AsyncWebServerRequest *req);
void formatFSHandler(AsyncWebServerRequest *req);
void fsInfoHandler(AsyncWebServerRequest *req);
void loadWiFiConfig();
void connectivityEndpoints();
void handleStopCapture(AsyncWebServerRequest *req);
void handleStartCapture(AsyncWebServerRequest *req);
void handleRestart(AsyncWebServerRequest *req);
void sendProgressUpdate();
void loadConfig();
void saveConfig();
void applyConfigFromDoc(JsonDocument &doc);
void publishTelemetryAndAlerts(const JsonDocument &doc);
 
 //---------------------------------------------------------------------
 // FILESYSTEM SETUP FUNCTIONS
 //---------------------------------------------------------------------
 void initFileSystem() {
   if (!LittleFS.begin()) {
     Serial.println("Failed to initialize file system");
     formatFileSystem();
   } else {
     Serial.println("File system initialized");
   }
 }
 
 void loadSensorLabels(JsonDocument &doc) {
   if (LittleFS.exists("/labels.json")) {
     File file = LittleFS.open("/labels.json", "r");
     if (file) {
       DeserializationError error = deserializeJson(doc, file);
       if (error) {
         Serial.println("Failed to read labels.json, initializing empty document");
         doc.to<JsonObject>();
       }
       file.close();
     }
   } else {
     doc.to<JsonObject>();
     saveSensorLabels(doc);
   }
 }
 
 void saveSensorLabels(JsonDocument &doc) {
   File file = LittleFS.open("/labels.json", "w");
   if (file) {
     serializeJson(doc, file);
     file.close();
     Serial.println("Sensor labels saved successfully");
   } else {
     Serial.println("Failed to write labels.json");
   }
 }
 
 void formatFileSystem() {
   Serial.println("Formatting file system...");
   if (LittleFS.format()) {
     Serial.println("File system formatted successfully");
     if (LittleFS.begin()) {
       Serial.println("File system initialized after formatting");
     } else {
       Serial.println("Failed to initialize file system after formatting");
     }
   } else {
     Serial.println("Failed to format file system");
   }
 }
 
// Wi-Fi event hook to drive MQTT presence and timers
void onWiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case SYSTEM_EVENT_STA_GOT_IP:
#ifdef ARDUINO_EVENT_WIFI_STA_GOT_IP
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
#endif
      // When we have IP, (re)connect MQTT
      mqttConnect();
      break;

    case SYSTEM_EVENT_STA_DISCONNECTED:
#ifdef ARDUINO_EVENT_WIFI_STA_DISCONNECTED
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
#endif
      // Nothing to do: LWT will publish OFFLINE automatically if broker connection drops.
      break;

    default: break;
  }
}
//---------------------------------------------------------------------
// WIFI CONFIGURATION FUNCTIONS
 //---------------------------------------------------------------------
 void loadWiFiConfig() {
   if (LittleFS.exists("/wifi_config.json")) {
     File file = LittleFS.open("/wifi_config.json", "r");
     if (file) {
       JsonDocument doc;
       DeserializationError error = deserializeJson(doc, file);
       if (!error) {
         if (!doc["ssid"].isNull() && !doc["password"].isNull()) {
           wifiSSID = doc["ssid"].as<String>();
           wifiPassword = doc["password"].as<String>();
           Serial.println("Loaded WiFi config: " + wifiSSID + ", " + wifiPassword);
         }
       }
       file.close();
     }
   }
 }
 
void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(hostName);
  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
  Serial.print("Connecting to WiFi");

  const uint32_t START = millis();
  const uint32_t TIMEOUT_MS = 30000; // try up to 30s before falling back to AP
  while (WiFi.status() != WL_CONNECTED && (millis() - START) < TIMEOUT_MS) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to WiFi. IP: " + WiFi.localIP().toString());
    isAPMode = false;
  } else {
    Serial.println("\nWiFi connection failed after 30s. Starting AP mode...");
    startAccessPoint();
  }
}
 
 //---------------------------------------------------------------------
 // WEBSOCKET FUNCTIONS
 //---------------------------------------------------------------------

void updateWebSocketClients() {
    String ipAddress = isAPMode ? WiFi.softAPIP().toString()
                                : WiFi.localIP().toString();

    String status = captureEnabled
                      ? (WiFi.status() == WL_CONNECTED ? "Connected to WiFi"
                                                       : "Storing data locally")
                      : "Paused";

    if (captureEnabled) sensors.requestTemperatures();

    int sensorCount = sensors.getDeviceCount();

    JsonDocument doc;
    doc["ipAddress"] = ipAddress;
    doc["status"]    = status;

    // ---- ESP status block ----
    JsonObject esp = doc["esp"].to<JsonObject>();
    esp["uptimeMs"] = (uint32_t)millis();

    JsonObject wifi = esp["wifi"].to<JsonObject>();
    wifi["connected"] = (WiFi.status() == WL_CONNECTED);
    wifi["rssi"]      = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : -127;
    wifi["ip"]        = ipAddress;

    uint32_t total = LittleFS.totalBytes();
    uint32_t used  = LittleFS.usedBytes();
    JsonObject fs  = esp["fs"].to<JsonObject>();
    fs["total"]   = total;
    fs["used"]    = used;
    fs["free"]    = (total > used) ? (total - used) : 0;
    fs["usedPct"] = total ? (uint32_t)((100.0 * used) / total) : 0;

    esp["mqttConnected"] = mqttIsConnected();

    // ---- Sensors array ----
    JsonArray sensorArray = doc["sensors"].to<JsonArray>();

    // Load label map once
    JsonDocument labelDoc;
    loadSensorLabels(labelDoc);
    JsonObject labels = labelDoc.as<JsonObject>();

    // DS18B20 temps (°F)
    for (int i = 0; i < sensorCount; ++i) {
        DeviceAddress deviceAddress;
        if (!sensors.getAddress(deviceAddress, i)) continue;

        char addressString[17];
        for (uint8_t j = 0; j < 8; ++j)
            sprintf(addressString + j * 2, "%02X", deviceAddress[j]);

        String label = labels[addressString].is<String>() ? labels[addressString].as<String>() : String("Sensor ") + String(i + 1);;
        String readingValue;
        float tempF = NAN;
        if (captureEnabled) {
            tempF = sensors.getTempF(deviceAddress);
            readingValue = isnan(tempF) ? "--" : String(tempF, 1) + "°F";
        } else {
            readingValue = "Paused";
        }

        JsonObject sensorObj = sensorArray.add<JsonObject>();
        sensorObj["id"]            = String(addressString);
        sensorObj["label"]         = "S" + String(i + 1) + ": " + label;
        sensorObj["reading_value"] = readingValue;
        if (!isnan(tempF)) sensorObj["tempF"] = tempF; // numeric for alerting
    }

    // DHT22 (throttled) humidity (%) and temp (°C)
    if (captureEnabled) {
        uint32_t now = millis();
        if (now - lastDhtRead >= DHT_MIN_INTERVAL_MS || isnan(lastHum) || isnan(lastTc)) {
            lastHum = dht.readHumidity();
            lastTc  = dht.readTemperature();
            lastDhtRead = now;
        }

        JsonObject h = sensorArray.add<JsonObject>();
        h["id"]            = "DHT22_Humidity";
        h["label"]         = "DHT22 Humidity";
        h["reading_value"] = isnan(lastHum) ? "--" : String(lastHum, 1) + "%";
        if (!isnan(lastHum)) h["humidity"] = lastHum;

        JsonObject t = sensorArray.add<JsonObject>();
        t["id"]            = "DHT22_TempC";
        t["label"]         = "DHT22 Temperature";
        t["reading_value"] = isnan(lastTc) ? "--" : String(lastTc, 1) + "°C";
        if (!isnan(lastTc)) t["tempC"] = lastTc;
    }

    // ---- Alerts ----
    JsonArray alerts = doc["alerts"].to<JsonArray>();
    // Capture state
    if (!captureEnabled) {
        JsonObject a = alerts.add<JsonObject>();
        a["severity"] = "warn";
        a["message"]  = "Data capture is paused.";
    }
    // WiFi / MQTT status
    if (WiFi.status() != WL_CONNECTED) {
        JsonObject a = alerts.add<JsonObject>();
        a["severity"] = "warn";
        a["message"]  = "WiFi disconnected – storing data locally.";
    }
    if (!mqttIsConnected()) {
        JsonObject a = alerts.add<JsonObject>();
        a["severity"] = "warn";
        a["message"]  = "MQTT broker not connected.";
    }
    // Filesystem usage
    uint32_t usedPct = total ? (uint32_t)((100.0 * used) / total) : 0;
    if (usedPct >= FS_USED_WARN_PCT) {
        JsonObject a = alerts.add<JsonObject>();
        a["severity"] = "warn";
        a["message"]  = String("Filesystem high usage: ") + String(usedPct) + "%";
    }
    // Sensor presence
    if (sensorCount == 0) {
        JsonObject a = alerts.add<JsonObject>();
        a["severity"] = "error";
        a["message"]  = "No DS18B20 sensors detected.";
    }
    // Threshold alerts from the DS18B20 readings
    for (JsonObject s : sensorArray) {
        if (s["tempF"].is<float>()) {
            float tf = s["tempF"].as<float>();
            if (tf > TEMP_F_HIGH || tf < TEMP_F_LOW) {
                JsonObject a = alerts.add<JsonObject>();
                a["severity"] = (tf > TEMP_F_HIGH) ? "error" : "warn";
                a["message"]  = String("Temperature out of range on ") + s["label"].as<String>()
                                + String(" (") + String(tf,1) + "°F)";
            }
        }
    }

    // DHT thresholds
    if (!isnan(lastHum) && (lastHum > HUMIDITY_HIGH || lastHum < HUMIDITY_LOW)) {
        JsonObject a = alerts.add<JsonObject>();
        a["severity"] = "warn";
        a["message"]  = String("Humidity out of range (") + String(lastHum,1) + "%)";
    }

    // ---- Broadcast ----
    String jsonData;
    serializeJson(doc, jsonData);
    ws.textAll(jsonData);
    publishTelemetryAndAlerts(doc);
}

 void handleWebSocketMessage(uint8_t num, uint8_t *payload, size_t length) {
   String message = String((char *)payload).substring(0, length);
   Serial.printf("WebSocket[%u]: %s\n", num, message.c_str());
 }
 
void onWebSocketEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type,
                     void * arg,uint8_t * data,size_t len)
{
  switch (type) {
    case WS_EVT_CONNECT: {
      Serial.printf("WebSocket client #%u connected\n", client->id());
      client->text("{\"status\":\"Connected to ESP32 WebSocket\"}");
      break;
    }
    case WS_EVT_DISCONNECT: {
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    }
    case WS_EVT_DATA: {
      AwsFrameInfo * info = (AwsFrameInfo*)arg;
      if (info->opcode == WS_TEXT) {
        // make sure payload is null-terminated
        if (len < 2048) {
          data[len] = '\0';
          handleWebSocketMessage(client->id(), data, len);
        }
      }
      break;
    }
    default:
      break;
  }
}

 
 //---------------------------------------------------------------------
 // STOP/START CAPTURE AND RESTART ENDPOINTS
 //---------------------------------------------------------------------
 // Stop sensor capture endpoint (pause data collection)
 void handleStopCapture(AsyncWebServerRequest *req) {
     captureEnabled = false;
     req->send(200, "text/plain", "Sensor data capture paused.");
 }
 
 // Start sensor capture endpoint (resume data collection)
 void handleStartCapture(AsyncWebServerRequest *req) {
     captureEnabled = true;
     req->send(200, "text/plain", "Sensor data capture resumed.");
 }
 
 //---------------------------------------------------------------------
 // OTA UPDATE FUNCTIONS
 //---------------------------------------------------------------------
 
void setupOTA()
{
  // /login
  static const char loginPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head>
  <title>Login</title>
  <style>
    body{font-family:Arial,sans-serif;text-align:center;background:#f4f4f4;}
    h1{color:#333;}
    .box{max-width:300px;margin:auto;padding:20px;background:#fff;border-radius:5px;box-shadow:0 0 10px #ccc;}
    .box input{width:90%;padding:10px;margin:10px 0;border:1px solid #ddd;border-radius:5px;font-size:1em;}
    .box input[type=button]{width:95%;background:#3498db;color:#fff;border:none;cursor:pointer;}
    .box input[type=button]:hover{background:#2377a5;}
  </style>
</head><body>
  <h1>Login</h1>
  <div class="box">
    <form name="loginForm">
      <input name="userid" placeholder="User ID" type="text">
      <input name="pwd"   placeholder="Password" type="password">
      <input type="button" value="Login" onclick="check(this.form)">
    </form>
  </div>
  <script>
    function check(f){
      if(f.userid.value==='admin' && f.pwd.value==='admin') location.href='/serverIndex';
      else alert('Error: Incorrect Username or Password');
    }
  </script>
</body></html>
)rawliteral";

  server.on("/login", HTTP_GET, [](AsyncWebServerRequest *req){
    req->send(200, "text/html; charset=utf-8", loginPage);   
  });

  // /serverIndex (UI)
  static const char otaPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head>
  <title>OTA Update</title>
  <style>
    body{font-family:Arial,sans-serif;text-align:center;background:#f4f4f4;}
    .progress-bar{width:50%;margin:50px auto;background:#ddd;height:20px;border-radius:10px;overflow:hidden;}
    .progress{height:100%;width:0%;}
    .message{margin-top:20px;font-size:1.2em;}
  </style>
</head><body>
  <h1>OTA Update</h1>
  <form method="POST" action="/update" enctype="multipart/form-data">
      <input type="file" name="update">
      <input type="submit" value="Update">
  </form>
  <div id="updateSection" style="display:none;">
     <h1>Updating firmware…</h1>
     <div class="progress-bar"><div class="progress" id="progress"></div></div>
     <p class="message" id="message">Waiting for update…</p>
  </div>
  <script>
    const ws=new WebSocket(`ws://${location.host}/ws`);
    ws.onmessage=e=>{
      const d=JSON.parse(e.data);
      if(d.progress!==undefined) document.getElementById('progress').style.width=d.progress+'%';
      if(d.message) document.getElementById('message').textContent=d.message;
      if(d.finished) setTimeout(()=>location.href='/',3000);
    };
    document.querySelector('form').addEventListener('submit',()=>{
      document.querySelector('form').style.display='none';
      document.getElementById('updateSection').style.display='block';
    });
  </script>
</body></html>
)rawliteral";

  server.on("/serverIndex", HTTP_GET, [](AsyncWebServerRequest *req){
    req->send(200, "text/html; charset=utf-8", otaPage);  
  });

  // /update (upload endpoint)
  server.on("/update", HTTP_POST,
    // 1) request-finished handler
    [](AsyncWebServerRequest *req)
    {
      static const char page[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="utf-8"><title>Update</title>
<style>body{font-family:Arial;text-align:center;background:#f4f4f4}</style>
</head><body><h1>Upload complete. Processing firmware…</h1></body></html>
)rawliteral";
      auto *resp = req->beginResponse(200, "text/html; charset=utf-8", page); 
      resp->addHeader("Connection", "close");
      req->send(resp);
    },
    // 2) chunk handler
    [](AsyncWebServerRequest *req, const String &filename, size_t index, uint8_t *data, size_t len, bool final)
    {
      static size_t bytesWritten = 0;
      static size_t totalSize    = 0;

      if (index == 0) {
        Serial.printf("Update start: %s\n", filename.c_str());
        totalSize        = req->contentLength();
        bytesWritten     = 0;
        uploadProgress   = 0;
        updateFinished   = false;
        updateSuccessful = false;
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
      }

      size_t written = Update.write(data, len);
      if (written != len) Update.printError(Serial);
      bytesWritten += written;

      uploadProgress = (int)(100.0 * bytesWritten / (totalSize ? totalSize : 1));
      sendProgressUpdate();

      if (final) {
        bool ok = Update.end(true);
        Serial.printf("Update %s: %u bytes\n", ok ? "SUCCESS" : "FAILED", (unsigned)bytesWritten);
        updateSuccessful = ok;
        uploadProgress   = 100;
        updateFinished   = true;
        sendProgressUpdate();
        delay(1500);
        ESP.restart();
      }
    },
    // 3) no file-abort callback
    nullptr
  );
}

 //---------------------------------------------------------------------
 // FILESYSTEM MANAGEMENT ENDPOINTS
 //---------------------------------------------------------------------
 void serverFileListHandler(AsyncWebServerRequest *req) {
  JsonDocument doc;
  JsonArray files = doc["files"].to<JsonArray>();

  File root = LittleFS.open("/");
  File file = root.openNextFile();
  while (file) {
    JsonObject f = files.add<JsonObject>();
    f["name"] = file.name();
    f["size"] = file.size();
    file = root.openNextFile();
  }

  String output;
  serializeJson(doc, output);
  req->send(200, "application/json", output);
}
 
 // Download handler for AsyncWebServer
void downloadFileHandler(AsyncWebServerRequest *req) {
  if (!req->hasParam("file")) {
    req->send(400, "text/plain", "Missing file parameter");
    return;
  }

  String filename = req->getParam("file")->value();
  if (!filename.startsWith("/")) {
    filename = "/" + filename;
  }

  if (!LittleFS.exists(filename)) {
    req->send(404, "text/plain", "File Not Found");
    return;
  }
  String downloadName = filename.substring(1);
AsyncWebServerResponse *resp =
    req->beginResponse(LittleFS, filename, "application/octet-stream", true);
resp->addHeader("Content-Disposition", "attachment; filename=\"" + downloadName + "\"");
req->send(resp);
}
 // standalone handler
void viewFileHandler(AsyncWebServerRequest *req) {
  if (!req->hasParam("file")) {
    req->send(400, "text/plain", "Missing file parameter");
    return;
  }

  String filename = req->getParam("file")->value();
  if (!filename.startsWith("/")) filename = "/" + filename;
  if (!LittleFS.exists(filename)) {
    req->send(404, "text/plain", "File Not Found");
    return;
  }

  File f = LittleFS.open(filename, "r");
  String content;
  while (f.available()) content += char(f.read());
  f.close();

  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>View File</title></head><body>"
                "<h1>Contents of " + filename + "</h1><pre>" + content + "</pre>"
                "<a href='/fs'>Back to Filesystem</a>"
                "</body></html>";
  req->send(200, "text/html", html);
  //server.on("/view-file", HTTP_GET, viewFileHandler);
// server.on("/download", HTTP_GET, downloadFileHandler);

}

 void handleFileUpload(AsyncWebServerRequest *req, const String &filename,size_t index,uint8_t *data,size_t len,bool final)
{
    static File fsUploadFile;

    if (index == 0) {
        String path = filename.startsWith("/") ? filename : "/" + filename;
        fsUploadFile = LittleFS.open(path, "w");
    }
    if (fsUploadFile) {
        if (len) fsUploadFile.write(data, len);
        if (final) {
            fsUploadFile.close();
            req->send(200, "text/plain", "Upload OK");
        }
    }
}
 void formatFSHandler(AsyncWebServerRequest *req){
   if (LittleFS.format()) {
     if (LittleFS.begin()) {
       req->send(200, "text/plain", "Filesystem formatted and reinitialized");
       return;
     }
   }
   req->send(500, "text/plain", "Filesystem formatting failed");
 }
 
 void fsInfoHandler(AsyncWebServerRequest *req) {
   JsonDocument doc;
   uint32_t total = LittleFS.totalBytes();
   uint32_t used = LittleFS.usedBytes();
   uint32_t free = total - used;
   doc["total"] = total;
   doc["used"] = used;
   doc["free"] = free;
   String json;
   serializeJson(doc, json);
   req->send(200, "application/json", json);
 }
 
 //---------------------------------------------------------------------
 // DATA/DB FUNCTIONS
 //---------------------------------------------------------------------
 void uploadStoredData() {
     if (WiFi.status() == WL_CONNECTED) {
         File file = LittleFS.open("/data.json", "r");
         if (!file) {
             Serial.println("No data to upload");
             return;
         }
         HTTPClient http;
         http.begin(remoteServerName);
         http.addHeader("Content-Type", "application/x-www-form-urlencoded");
         JsonDocument doc;
         JsonArray data = doc["data"].to<JsonArray>();
         while (file.available()) {
             String line = file.readStringUntil('\n');
             JsonDocument tempDoc;
             DeserializationError error = deserializeJson(tempDoc, line);
             if (!error) {
                 data.add(tempDoc.as<JsonObject>());
             }
         }
         file.close();
         String jsonData;
         serializeJson(doc, jsonData);
         String httpRequestData = "api_key=" + String(remoteApiKey) + "&data=" + jsonData;
         int httpResponseCode = http.POST(httpRequestData);
         if (httpResponseCode == 200) {
             Serial.println("Stored data uploaded successfully");
         } else {
             Serial.printf("Error uploading stored data: %d\n", httpResponseCode);
         }
         http.end();
     }
 }
 
 void sendSensorData() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(remoteServerName);
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");

        JsonDocument doc;
        JsonArray data = doc["data"].to<JsonArray>();

        sensors.requestTemperatures();
        int sensorCount = sensors.getDeviceCount();
        DeviceAddress addr;

        for (int i = 0; i < sensorCount; i++) {
            if (!sensors.getAddress(addr, i)) continue;
            float tempF = sensors.getTempF(addr);
            String reading = isnan(tempF) ? "--" : String(tempF, 1);

            char addrStr[17];
            for (uint8_t j = 0; j < 8; j++) {
                sprintf(addrStr + j * 2, "%02X", addr[j]);
            }

            JsonObject sensorData = data.add<JsonObject>();
            sensorData["sensor_id"] = String(addrStr);
            sensorData["reading_value"] = reading;
        }

        String jsonData;
        serializeJson(doc, jsonData);
        String httpRequestData = "api_key=" + String(remoteApiKey) + "&data=" + jsonData;

        int httpResponseCode = http.POST(httpRequestData);
        http.end();

        if (httpResponseCode == 200) {
            Serial.println("Data sent successfully");
        } else {
            Serial.printf("Error sending data: %d\n", httpResponseCode);
        }
    } else {
        storeDataLocally();
    }
}

 
 void storeLabelsLocally(JsonDocument &doc) {
   File file = LittleFS.open("/labels.json", "w");
   if (file) {
     serializeJson(doc, file);
     file.close();
     Serial.println("Labels stored locally");
   } else {
     Serial.println("Failed to open labels.json file");
   }
 }
 
 void storeDataLocally() {
    if (!LittleFS.exists("/data.json")) {
        File file = LittleFS.open("/data.json", "w");
        file.close();
    }
    File file = LittleFS.open("/data.json", "a");
    if (file) {
        sensors.requestTemperatures();
        DeviceAddress addr;
        if (sensors.getAddress(addr, 0)) {
            float tempF = sensors.getTempF(addr);
            String reading = isnan(tempF) ? "--" : String(tempF, 1);
            char buf[17];
            for (uint8_t i = 0; i < 8; i++) {
                sprintf(buf + i * 2, "%02X", addr[i]);
            }
            JsonDocument doc;
            JsonObject sensorData = doc.to<JsonObject>();
            sensorData["timestamp"]     = millis();
            sensorData["sensor_id"]     = String(buf);
            sensorData["reading_value"] = reading;
            String jsonData;
            serializeJson(doc, jsonData);
            file.println(jsonData);
        }
        file.close();
    }
}

 
 //---------------------------------------------------------------------
 // WIFI CONNECTIVITY ENDPOINTS
 //---------------------------------------------------------------------
 void connectivityEndpoints() {
  // Endpoint to disconnect WiFi and force AP mode.
  server.on("/disconnect", HTTP_POST, [](AsyncWebServerRequest *req) {
    WiFi.disconnect();
    startAccessPoint();
    req->send(200, "text/plain", "WiFi disconnected. Now in AP Mode.");
  });

  // Endpoint to scan for available networks.
  server.on("/scan", HTTP_GET, [](AsyncWebServerRequest *req) {
  int n = WiFi.scanNetworks();
  JsonDocument doc;
  JsonArray arr = doc["networks"].to<JsonArray>();
  if (n > 0) {
    for (int i = 0; i < n; i++) {
      JsonObject net = arr.add<JsonObject>();
      net["ssid"] = WiFi.SSID(i);
      net["rssi"] = WiFi.RSSI(i);
    }
  }
  String output;
  serializeJson(doc, output);
  req->send(200, "application/json", output);
});

  // Endpoint to accept new WiFi credentials and attempt reconnection.
  server.on("/connectivity", HTTP_POST, [](AsyncWebServerRequest *req) {
    if (!req->hasArg("plain")) {
      req->send(400, "text/plain", "No data provided.");
      return;
    }
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, req->arg("plain"));
    if (error) {
      req->send(400, "text/plain", "Invalid JSON data.");
      return;
    }
    if (!doc["ssid"] || !doc["password"]) {
      req->send(400, "text/plain", "Missing ssid or password.");
      return;
    }
    wifiSSID = doc["ssid"].as<String>();
    wifiPassword = doc["password"].as<String>();

    JsonDocument wifiDoc;
    wifiDoc["ssid"]     = wifiSSID;
    wifiDoc["password"] = wifiPassword;
    File wifiFile = LittleFS.open("/wifi_config.json", "w");
    if (wifiFile) {
      serializeJson(wifiDoc, wifiFile);
      wifiFile.close();
    }

    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
      delay(500);
    }
    if (WiFi.status() == WL_CONNECTED) {
      req->send(200, "text/plain", "Connected to WiFi: " + wifiSSID);
    } else {
      req->send(200, "text/plain", "Failed to connect to WiFi: " + wifiSSID);
    }
  });

  // Serve the Connectivity management page.
  server.on("/connectivity", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send(200, "text/html", connectivityPage);
  });
}

 //---------------------------------------------------------------------
 // START ACCESS POINT (AP MODE)
 //---------------------------------------------------------------------
 void startAccessPoint() {
   WiFi.mode(WIFI_AP);
   WiFi.softAP("ESP32_AP");
   isAPMode = true;
   Serial.println("AP Mode started. IP: " + WiFi.softAPIP().toString());

 }
 
 // Restart the device endpoint
 void handleRestart(AsyncWebServerRequest *req) {
     req->send(200, "text/plain", "Restarting device...");
     delay(1000);  // Give time for response to be sent
     ESP.restart();
 }
 
 // Helper function to send OTA progress updates over the websocket.
 void sendProgressUpdate() {
   String msg = "{";
   msg += "\"progress\":" + String(uploadProgress) + ",";
   if (updateFinished) {
     if (updateSuccessful)
       msg += "\"message\":\"Update complete. Rebooting...\",";
     else
       msg += "\"message\":\"Update failed. Please try again.\",";
   } else {
     msg += "\"message\":\"Uploading...\",";
   }
   msg += "\"finished\":" + String(updateFinished ? "true" : "false");
   msg += "}";
   ws.textAll(msg);
 }
 
 //---------------------------------------------------------------------
// MAIN SETUP FUNCTION
//---------------------------------------------------------------------
void setup() {

  // boot + init
  Serial.begin(500000);
  Serial.println("Booting…");
  Serial.println("Firmware Version: " + String(FIRMWARE_VERSION));
  sensors.begin();
  dht.begin();
  initFileSystem();
  loadWiFiConfig();
  // Register Wi-Fi events and prime MQTT topic base
  WiFi.onEvent(onWiFiEvent);
  mqttBaseTopic = String("dev/") + deviceIdNoColon();
  connectToWiFi();
  setupOTA();
  connectivityEndpoints();

  // page routes
  server.on("/manage",       HTTP_GET, [](AsyncWebServerRequest *req){ req->send(200,"text/html",managePage); });
  server.on("/labels",       HTTP_GET, [](AsyncWebServerRequest *req){ req->send(200,"text/html",labelsPage); });
  server.on("/fs",           HTTP_GET, [](AsyncWebServerRequest *req){ req->send(200,"text/html",fsPage); });
  //server.on("/connectivity", HTTP_GET, [](AsyncWebServerRequest *req){ req->send_P(200,"text/html",connectivityPage); });

  // update sensor labels
  server.on("/update-labels", HTTP_POST,
    [](AsyncWebServerRequest *req){},nullptr,
    [](AsyncWebServerRequest *req,uint8_t *data,size_t len,size_t, size_t){
      JsonDocument doc;
      if (deserializeJson(doc, String((char*)data, len))) {
        req->send(400,"application/json","{\"error\":\"Invalid JSON\"}");return;
      }
      saveSensorLabels(doc);
      req->send(200,"application/json","{\"status\":\"success\"}");
    });

  // sensors JSON
  server.on("/get-sensors", HTTP_GET, [](AsyncWebServerRequest *req){
    sensors.requestTemperatures();
    int cnt = sensors.getDeviceCount();
    JsonDocument lbl;
    loadSensorLabels(lbl);
    JsonDocument out;
    JsonArray arr = out["sensors"].to<JsonArray>();

    // DS18B20 sensors
    for (int i = 0; i < cnt; i++) {
      DeviceAddress a;
      if (!sensors.getAddress(a, i)) continue;

      char id[17];
      for (uint8_t j = 0; j < 8; j++) sprintf(id + j * 2, "%02X", a[j]);
      id[16] = '\0'; 

      String label = lbl[id].is<String>() ? lbl[id].as<String>() : "Sensor " + String(i + 1);
      float tempF  = sensors.getTempF(a);
      String reading = isnan(tempF) ? "--" : String(tempF, 2) + " °F";

      JsonObject o = arr.add<JsonObject>();
      o["id"]            = id;
      o["label"]         = "S" + String(i + 1) + ": " + label;
      o["reading_value"] = reading;
      if (!isnan(tempF)) o["tempF"] = tempF;  // numeric field for alerting/UI
    }

    //  DHT22 readings
    {
      float hum = dht.readHumidity();
      float tc  = dht.readTemperature();  // °C

      JsonObject h = arr.add<JsonObject>();
      h["id"]            = "DHT22_Humidity";
      h["label"]         = "DHT22 Humidity";
      h["reading_value"] = isnan(hum) ? "--" : String(hum, 1) + "%";
      if (!isnan(hum)) h["humidity"] = hum;   // numeric value

      JsonObject t = arr.add<JsonObject>();
      t["id"]            = "DHT22_TempC";
      t["label"]         = "DHT22 Temperature";
      t["reading_value"] = isnan(tc) ? "--" : String(tc, 1) + " °C";
      if (!isnan(tc)) t["tempC"] = tc;        // numeric value
    }
    String json; serializeJson(out, json);
    req->send(200, "application/json", json);
  });

  // lightweight status JSON for UI polling
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *req){
    JsonDocument st;

    const bool staUp = (WiFi.status() == WL_CONNECTED);
    const String ipAddress = isAPMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();

    st["ipAddress"] = ipAddress;
    st["status"]    = captureEnabled
                        ? (staUp ? "Connected to WiFi" : (isAPMode ? "AP Mode" : "Offline"))
                        : "Paused";

    JsonObject esp = st["esp"].to<JsonObject>();
    esp["uptimeMs"] = (uint32_t)millis();

    JsonObject wifi = esp["wifi"].to<JsonObject>();
    wifi["connected"] = staUp;
    wifi["rssi"]      = staUp ? WiFi.RSSI() : -127;
    wifi["ip"]        = ipAddress;

    const uint32_t total = LittleFS.totalBytes();
    const uint32_t used  = LittleFS.usedBytes();
    JsonObject fs = esp["fs"].to<JsonObject>();
    fs["total"]   = total;
    fs["used"]    = used;
    fs["free"]    = (total > used) ? (total - used) : 0;
    fs["usedPct"] = total ? (uint32_t)((100.0 * used) / total) : 0;

    esp["mqttConnected"] = mqttIsConnected();

    String out; serializeJson(st, out);
    req->send(200, "application/json", out);
  });

  // filesystem & capture
  server.on("/list-files",   HTTP_GET, serverFileListHandler);
  server.on("/download",     HTTP_GET, downloadFileHandler);
  server.on("/view-file",    HTTP_GET, viewFileHandler);
  server.on("/delete-file",  HTTP_GET, [](AsyncWebServerRequest *req){
    if(!req->hasParam("file")){ req->send(400,"text/plain","Missing file parameter"); return; }
    String fn=req->getParam("file")->value(); if(!fn.startsWith("/")) fn="/"+fn;
    if(!LittleFS.exists(fn)){ req->send(404,"text/plain","File Not Found"); return; }
    bool ok = LittleFS.remove(fn);
    req->send(ok?200:500,"text/plain", ok?"File deleted successfully":"Failed to delete file");
  });
  server.on("/upload-file",  HTTP_POST, [](AsyncWebServerRequest *req){ req->send(200,"text/plain","File Uploaded"); }, handleFileUpload);
  server.on("/format-fs",    HTTP_GET, formatFSHandler);
  server.on("/fsinfo",       HTTP_GET, fsInfoHandler);
  server.on("/stop-capture", HTTP_POST, handleStopCapture);
  server.on("/start-capture",HTTP_POST, handleStartCapture);
  server.on("/restart",      HTTP_POST, handleRestart);
  //server.on("/upload-file", HTTP_POST, [](AsyncWebServerRequest *req){ 
    //req->send(200,"text/plain","OK"); }, handleFileUpload, nullptr);
 
    // main page + websocket
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req){ req->send(200, "text/html; charset=utf-8", webpage, processor); });
  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);
  server.begin();
  updateWebSocketClients();

  if (mqttIsConnected()) {
    JsonDocument hb;
    buildHeartbeatDoc(hb, "BOOTED");
    mqttPublish("/heartbeat", hb, false, 1);
    mqttPublishRaw("/critical", "online", true, 1);
  }
}

 //---------------------------------------------------------------------
 // MAIN LOOP FUNCTION
 //---------------------------------------------------------------------
void loop() {
    ws.cleanupClients();
  // Call updateWebSocketClients() every webSocketInterval
  if (millis() - lastWebSocketTime > webSocketInterval) {
      updateWebSocketClients();
      lastWebSocketTime = millis();
  }

  // Every dbUpdateInterval, if capture is enabled, store and possibly upload sensor data.
  if (captureEnabled && millis() - lastDBUpdateTime > dbUpdateInterval) {
      storeDataLocally();
//      if (WiFi.status() == WL_CONNECTED) {
//          uploadStoredData();
//      }
      lastDBUpdateTime = millis();
  }

 // ---- MQTT timed publishers ----
 if (mqttIsConnected()) {
   unsigned long now = millis();
   if (now - lastMqttHeartbeat >= HEARTBEAT_MS) {
     lastMqttHeartbeat = now;
     JsonDocument hb;
     buildHeartbeatDoc(hb, "ONLINE");
     mqttPublish("/heartbeat", hb, true, 1);  // retained snapshot
   }
   if (now - lastMqttHealth >= HEALTH_MS) {
     lastMqttHealth = now;
     JsonDocument health;
     buildHealthDoc(health);
     mqttPublish("/health", health, false, 0);
   }
 }
}