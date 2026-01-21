/*
 * ESP-01S WiFi CAT Bridge for Kenwood TS-440S
 * 
 * Features:
 * - TCP bridge on port 7373 for rigctl/WSJT-X
 * - Web interface on port 80 for frequency/mode control
 * - OTA firmware updates
 * - WiFiManager for easy WiFi setup
 * - mDNS (ts440-bridge.local)
 * 
 * Hardware:
 * - ESP-01S (1MB flash)
 * - MAX3232 level shifter
 * - TS-440S IF-232C interface
 * 
 * Wiring:
 * ESP-01S TX -> MAX3232 TTL RX -> IF-232C RX
 * ESP-01S RX -> MAX3232 TTL TX -> IF-232C TX
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiManager.h>
#include <ArduinoOTA.h>

// ============= CONFIGURATION =============
#define HOSTNAME "ts440-bridge"
#define TCP_PORT 7373          // Port for rigctl/WSJT-X
#define SERIAL_BAUD 4800       // TS-440S default (change to 9600 if needed)
#define MAX_CLIENTS 1          // Only allow one TCP client at a time
#define COMMAND_TIMEOUT 1000   // Max time to wait for radio response (ms)

// ============= GLOBAL OBJECTS =============
ESP8266WebServer webServer(80);
WiFiServer tcpServer(TCP_PORT);
WiFiClient tcpClient;

// ============= STATE VARIABLES =============
String currentFrequency = "14.074000";  // Default display
String currentMode = "USB";
unsigned long lastRadioUpdate = 0;
#define UPDATE_INTERVAL 2000  // Update from radio every 2 seconds

// ============= CAT COMMAND FUNCTIONS =============

// Send command to radio and wait for response
String sendCATCommand(String command) {
  Serial.print(command);
  Serial.flush();
  
  unsigned long startTime = millis();
  String response = "";
  
  while (millis() - startTime < COMMAND_TIMEOUT) {
    if (Serial.available()) {
      char c = Serial.read();
      response += c;
      if (c == ';') break;  // TS-440S commands end with semicolon
    }
    yield();
  }
  
  return response;
}

// Read frequency from radio (FA command)
String readFrequency() {
  String response = sendCATCommand("FA;");
  if (response.length() >= 13 && response.startsWith("FA")) {
    // Response format: FAxxxxxxxxxx; (11 digits for Hz)
    String freqHz = response.substring(2, 13);
    // Convert to MHz with 6 decimals
    long freq = freqHz.toInt();
    float freqMHz = freq / 1000000.0;
    return String(freqMHz, 6);
  }
  return currentFrequency;  // Return last known if failed
}

// Set frequency (FA command)
bool setFrequency(String freqMHz) {
  // Convert MHz to Hz
  float freq = freqMHz.toFloat();
  long freqHz = (long)(freq * 1000000);
  
  // Format: FAxxxxxxxxxx; (11 digits, zero-padded)
  char cmd[16];
  sprintf(cmd, "FA%011ld;", freqHz);
  
  String response = sendCATCommand(cmd);
  return response.startsWith("FA");
}

// Read mode from radio (MD command)
String readMode() {
  String response = sendCATCommand("MD;");
  if (response.length() >= 4 && response.startsWith("MD")) {
    char mode = response.charAt(2);
    switch(mode) {
      case '1': return "LSB";
      case '2': return "USB";
      case '3': return "CW";
      case '4': return "FM";
      case '5': return "AM";
      case '6': return "FSK";
      default: return "USB";
    }
  }
  return currentMode;  // Return last known if failed
}

// Set mode (MD command)
bool setMode(String mode) {
  char modeChar = '2';  // Default USB
  
  if (mode == "LSB") modeChar = '1';
  else if (mode == "USB") modeChar = '2';
  else if (mode == "CW") modeChar = '3';
  else if (mode == "FM") modeChar = '4';
  else if (mode == "AM") modeChar = '5';
  else if (mode == "FSK") modeChar = '6';
  
  String cmd = "MD" + String(modeChar) + ";";
  String response = sendCATCommand(cmd);
  return response.startsWith("MD");
}

// Update current status from radio
void updateRadioStatus() {
  if (millis() - lastRadioUpdate > UPDATE_INTERVAL) {
    currentFrequency = readFrequency();
    currentMode = readMode();
    lastRadioUpdate = millis();
  }
}

// ============= WEB SERVER HANDLERS =============

void handleRoot() {
  updateRadioStatus();  // Get latest status before showing page
  
  String html = F("<!DOCTYPE html><html><head><meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>TS-440S Control</title>"
    "<style>"
    "body{font-family:Arial,sans-serif;background:#1a1a1a;color:#e0e0e0;margin:0;padding:20px}"
    ".container{max-width:600px;margin:0 auto;background:#2d2d2d;padding:30px;border-radius:10px;box-shadow:0 4px 6px rgba(0,0,0,0.3)}"
    "h1{color:#4a9eff;text-align:center;margin-top:0}"
    ".status{background:#1a1a1a;padding:20px;border-radius:5px;margin-bottom:20px;text-align:center}"
    ".freq{font-size:2.5em;font-weight:bold;color:#4a9eff;margin:10px 0}"
    ".mode{font-size:1.5em;color:#ffa500;margin:10px 0}"
    ".control-group{margin:20px 0}"
    "label{display:block;margin-bottom:5px;color:#b0b0b0;font-weight:bold}"
    "input,select{width:100%;padding:12px;font-size:1.1em;background:#1a1a1a;color:#e0e0e0;border:2px solid #4a9eff;border-radius:5px;box-sizing:border-box}"
    "button{width:100%;padding:15px;font-size:1.1em;background:#4a9eff;color:#fff;border:none;border-radius:5px;cursor:pointer;margin-top:10px;font-weight:bold}"
    "button:hover{background:#3a8eef}"
    "button:active{background:#2a7edf}"
    ".memory-buttons{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-top:20px}"
    ".memory-buttons button{padding:10px;font-size:0.9em}"
    ".info{text-align:center;margin-top:20px;font-size:0.9em;color:#808080}"
    "</style>"
    "</head><body>"
    "<div class='container'>"
    "<h1>🎚️ TS-440S Control</h1>"
    "<div class='status'>"
    "<div class='freq' id='frequency'>");
  
  html += currentFrequency;
  html += F(" MHz</div>"
    "<div class='mode' id='mode'>");
  html += currentMode;
  html += F("</div></div>"
    
    "<div class='control-group'>"
    "<label>Frequency (MHz)</label>"
    "<input type='text' id='freqInput' placeholder='14.074' value='");
  html += currentFrequency;
  html += F("'>"
    "<button onclick='setFreq()'>Set Frequency</button>"
    "</div>"
    
    "<div class='control-group'>"
    "<label>Mode</label>"
    "<select id='modeSelect'>"
    "<option value='LSB'");
  if (currentMode == "LSB") html += F(" selected");
  html += F(">LSB</option>"
    "<option value='USB'");
  if (currentMode == "USB") html += F(" selected");
  html += F(">USB</option>"
    "<option value='CW'");
  if (currentMode == "CW") html += F(" selected");
  html += F(">CW</option>"
    "<option value='FM'");
  if (currentMode == "FM") html += F(" selected");
  html += F(">FM</option>"
    "<option value='AM'");
  if (currentMode == "AM") html += F(" selected");
  html += F(">AM</option>"
    "<option value='FSK'");
  if (currentMode == "FSK") html += F(" selected");
  html += F(">FSK</option>"
    "</select>"
    "<button onclick='setMode()'>Set Mode</button>"
    "</div>"
    
    "<div class='memory-buttons'>"
    "<button onclick='setQuickFreq(\"7.074\")'>40m FT8</button>"
    "<button onclick='setQuickFreq(\"14.074\")'>20m FT8</button>"
    "<button onclick='setQuickFreq(\"7.200\")'>40m SSB</button>"
    "<button onclick='setQuickFreq(\"14.200\")'>20m SSB</button>"
    "</div>"
    
    "<div class='info'>"
    "TCP Bridge: Port ");
  html += String(TCP_PORT);
  html += F(" | ");
  html += HOSTNAME;
  html += F(".local"
    "</div>"
    "</div>"
    
    "<script>"
    "function setFreq(){"
    "var freq=document.getElementById('freqInput').value;"
    "fetch('/setfreq?freq='+encodeURIComponent(freq))"
    ".then(r=>r.text()).then(d=>{alert(d);updateStatus();})"
    ".catch(e=>alert('Error: '+e));}"
    
    "function setMode(){"
    "var mode=document.getElementById('modeSelect').value;"
    "fetch('/setmode?mode='+encodeURIComponent(mode))"
    ".then(r=>r.text()).then(d=>{alert(d);updateStatus();})"
    ".catch(e=>alert('Error: '+e));}"
    
    "function setQuickFreq(freq){"
    "fetch('/setfreq?freq='+freq)"
    ".then(r=>r.text()).then(d=>{updateStatus();})"
    ".catch(e=>alert('Error: '+e));}"
    
    "function updateStatus(){"
    "fetch('/status').then(r=>r.json()).then(d=>{"
    "document.getElementById('frequency').innerText=d.frequency+' MHz';"
    "document.getElementById('mode').innerText=d.mode;"
    "document.getElementById('freqInput').value=d.frequency;"
    "document.getElementById('modeSelect').value=d.mode;});"
    "}"
    
    "setInterval(updateStatus,3000);"
    "</script>"
    "</body></html>");
  
  webServer.send(200, "text/html", html);
}

void handleSetFreq() {
  if (webServer.hasArg("freq")) {
    String freq = webServer.arg("freq");
    if (setFrequency(freq)) {
      currentFrequency = freq;
      webServer.send(200, "text/plain", "Frequency set to " + freq + " MHz");
    } else {
      webServer.send(500, "text/plain", "Failed to set frequency");
    }
  } else {
    webServer.send(400, "text/plain", "Missing frequency parameter");
  }
}

void handleSetMode() {
  if (webServer.hasArg("mode")) {
    String mode = webServer.arg("mode");
    if (setMode(mode)) {
      currentMode = mode;
      webServer.send(200, "text/plain", "Mode set to " + mode);
    } else {
      webServer.send(500, "text/plain", "Failed to set mode");
    }
  } else {
    webServer.send(400, "text/plain", "Missing mode parameter");
  }
}

void handleStatus() {
  updateRadioStatus();
  String json = "{\"frequency\":\"" + currentFrequency + "\",\"mode\":\"" + currentMode + "\"}";
  webServer.send(200, "application/json", json);
}

// ============= TCP BRIDGE FUNCTIONS =============

void handleTCPBridge() {
  // Accept new clients
  if (tcpServer.hasClient()) {
    if (!tcpClient || !tcpClient.connected()) {
      if (tcpClient) tcpClient.stop();
      tcpClient = tcpServer.available();
    } else {
      // Already have a client, reject new one
      WiFiClient rejectClient = tcpServer.available();
      rejectClient.stop();
    }
  }
  
  // Forward data from TCP to Serial
  if (tcpClient && tcpClient.connected()) {
    while (tcpClient.available()) {
      char c = tcpClient.read();
      Serial.write(c);
    }
  }
  
  // Forward data from Serial to TCP
  if (tcpClient && tcpClient.connected()) {
    while (Serial.available()) {
      char c = Serial.read();
      tcpClient.write(c);
    }
  }
}

// ============= SETUP =============

void setup() {
  // Initialize serial for radio
  Serial.begin(SERIAL_BAUD);
  Serial.setTimeout(COMMAND_TIMEOUT);
  
  delay(1000);
  
  // WiFiManager setup
  WiFiManager wm;
  
  // Uncomment to reset WiFi settings for testing:
  // wm.resetSettings();
  
  // Set custom hostname before connecting
  WiFi.hostname(HOSTNAME);
  
  // Auto-connect or start config portal
  bool connected = wm.autoConnect("TS440-Setup");
  
  if (!connected) {
    delay(3000);
    ESP.restart();
  }
  
  // Start mDNS
  if (MDNS.begin(HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("ts440cat", "tcp", TCP_PORT);
  }
  
  // Setup OTA
  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.onStart([]() {
    // Stop servers during OTA
    tcpServer.stop();
    if (tcpClient) tcpClient.stop();
  });
  ArduinoOTA.begin();
  
  // Start TCP server for rigctl/WSJT-X
  tcpServer.begin();
  tcpServer.setNoDelay(true);
  
  // Start web server
  webServer.on("/", handleRoot);
  webServer.on("/setfreq", handleSetFreq);
  webServer.on("/setmode", handleSetMode);
  webServer.on("/status", handleStatus);
  webServer.begin();
  
  // Initial radio status read
  delay(500);
  updateRadioStatus();
}

// ============= MAIN LOOP =============

void loop() {
  ArduinoOTA.handle();
  MDNS.update();
  webServer.handleClient();
  handleTCPBridge();
  yield();
}
