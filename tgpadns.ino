/*
 * MIT License
 *
 * Copyright (c) 2026 controllercustom@myyahoo.com
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <WiFiManager.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <Preferences.h>
#include <NSLiteController.h>

#ifdef ARDUINO_M5STACK_ATOMS3
#include <M5GFX.h>
#include <lgfx/v1/panel/Panel_GC9A01.hpp>
#include <lgfx/v1/platforms/esp32/Bus_SPI.hpp>
#include <lgfx/v1/platforms/esp32/Light_PWM.hpp>
#endif
#include <esp_wifi.h>

#define VERSION "1.0.0"

// Uncomment next line and change the password to enable OTA authentication:
// #define OTA_PASS "your-password-here"

NSLiteController Gamepad;
#ifdef ARDUINO_M5STACK_ATOMS3
M5GFX display;
#endif
WebServer server(80);
WebSocketsServer webSocket(81);

// ====================================================================
// State
// ====================================================================
#define MAX_WS_CLIENTS WEBSOCKETS_SERVER_CLIENT_MAX

struct ClientState {
  bool   active = false;
  uint32_t lastSeen = 0;
  bool   btn[16] = {false};
  int8_t lx = 0, ly = 0, rx = 0, ry = 0;
  uint8_t dpad = 0;                   // NS_DPAD_CENTERED = 0
  uint32_t axisTs = 0, dpadTs = 0;
};
static ClientState clients[MAX_WS_CLIENTS];

uint8_t btnRef[16];

uint8_t stickDeadzone = 0;

char hostname[33];
bool portalConfigSaved = false;
WiFiManagerParameter customHostnameParam("hostname", "Device hostname", "tgpadns", 32);

#ifdef ARDUINO_M5STACK_ATOMS3
#define RESET_BUTTON_PIN 41
#else
#define RESET_BUTTON_PIN 0
#endif
unsigned long resetPressStart = 0;
bool resetButtonWasLow = false;
bool anyWSActivity = false;
int wsClientCount = 0;

static void resetAllState() {
  Gamepad.releaseAll();
  memset(btnRef, 0, sizeof(btnRef));
  for (uint8_t i = 0; i < MAX_WS_CLIENTS; i++) {
    ClientState c;
    clients[i] = c;
  }
}

static void recomputeAndSend(unsigned long now) {
  // Buttons: OR across all active clients.
  memset(btnRef, 0, sizeof(btnRef));
  for (uint8_t b = 0; b < 14; b++) {
    for (uint8_t i = 0; i < MAX_WS_CLIENTS; i++) {
      if (clients[i].active && clients[i].btn[b]) btnRef[b]++;
    }
    Gamepad.setButton(b, btnRef[b] > 0);
  }

  // Axes / d-pad: freshest active contributor.
  int8_t lx = 0, ly = 0, rx = 0, ry = 0;
  uint8_t dpad = 0;
  uint32_t bestAxis = 0, bestDpad = 0;

  for (uint8_t i = 0; i < MAX_WS_CLIENTS; i++) {
    ClientState& c = clients[i];
    if (!c.active) continue;
    if (c.axisTs >= bestAxis) { bestAxis = c.axisTs; lx = c.lx; ly = c.ly; rx = c.rx; ry = c.ry; }
    if (c.dpadTs >= bestDpad) { bestDpad = c.dpadTs; dpad = c.dpad; }
  }

  // Scale WS -127..127 to NSLite -32768..32767
  Gamepad.setStickLeft((int16_t)lx * 258, (int16_t)ly * 258);
  Gamepad.setStickRight((int16_t)rx * 258, (int16_t)ry * 258);
  Gamepad.setDpad(dpad);
  Gamepad.send();
}

// ====================================================================
// Button handlers
// ====================================================================
// Map WS tokens to NS_BTN_* (Nintendo Switch Lite button order).
static uint8_t btnIndex(const char* key) {
  if (key[0] != '*') return 255;
  if (strcmp(key, "*X")  == 0) return NS_BTN_B;
  if (strcmp(key, "*O")  == 0) return NS_BTN_A;
  if (strcmp(key, "*Sq") == 0) return NS_BTN_Y;
  if (strcmp(key, "*Tr") == 0) return NS_BTN_X;
  if (strcmp(key, "*L1") == 0) return NS_BTN_L;
  if (strcmp(key, "*R1") == 0) return NS_BTN_R;
  if (strcmp(key, "*L2") == 0) return NS_BTN_ZL;
  if (strcmp(key, "*R2") == 0) return NS_BTN_ZR;
  if (strcmp(key, "*Sh") == 0) return NS_BTN_MINUS;
  if (strcmp(key, "*Op") == 0) return NS_BTN_PLUS;
  if (strcmp(key, "*L3") == 0) return NS_BTN_LCLICK;
  if (strcmp(key, "*R3") == 0) return NS_BTN_RCLICK;
  if (strcmp(key, "*Ps") == 0) return NS_BTN_HOME;
  if (strcmp(key, "*Tp") == 0) return NS_BTN_CAPTURE;
  return 255;
}

static void handleKeyDown(uint8_t num, const char* key, unsigned long now) {
  if (num >= MAX_WS_CLIENTS) return;
  ClientState& c = clients[num];
  c.active = true;
  c.lastSeen = now;
  anyWSActivity = true;

  uint8_t b = btnIndex(key);
  if (b != 255) { c.btn[b] = true; recomputeAndSend(now); return; }

  // D-pad (WS value 0..7 direction, 8=centered → NSLite 0=centered, 1..8=direction)
  if (strncmp(key, "*DPAD:", 6) == 0) {
    int v = atoi(key + 6);
    c.dpad = (v >= 0 && v <= 7) ? (uint8_t)(v + 1) : 0;
    c.dpadTs = now;
    recomputeAndSend(now);
    return;
  }

  // Analog sticks (WS -127..127)
  if (strncmp(key, "*LX:", 4) == 0) { c.lx = (int8_t)atoi(key + 4); c.axisTs = now; recomputeAndSend(now); return; }
  if (strncmp(key, "*LY:", 4) == 0) { c.ly = (int8_t)atoi(key + 4); c.axisTs = now; recomputeAndSend(now); return; }
  if (strncmp(key, "*RX:", 4) == 0) { c.rx = (int8_t)atoi(key + 4); c.axisTs = now; recomputeAndSend(now); return; }
  if (strncmp(key, "*RY:", 4) == 0) { c.ry = (int8_t)atoi(key + 4); c.axisTs = now; recomputeAndSend(now); return; }
}

static void handleKeyUp(uint8_t num, const char* key, unsigned long now) {
  if (num >= MAX_WS_CLIENTS) return;
  ClientState& c = clients[num];
  c.lastSeen = now;

  uint8_t b = btnIndex(key);
  if (b != 255) { c.btn[b] = false; recomputeAndSend(now); return; }

  if (strncmp(key, "*DPAD:", 6) == 0) { c.dpad = 0; c.dpadTs = now; recomputeAndSend(now); return; }
  if (strncmp(key, "*LX", 3) == 0 && strlen(key) <= 5) { c.lx = 0; c.axisTs = now; recomputeAndSend(now); return; }
  if (strncmp(key, "*LY", 3) == 0 && strlen(key) <= 5) { c.ly = 0; c.axisTs = now; recomputeAndSend(now); return; }
  if (strncmp(key, "*RX", 3) == 0 && strlen(key) <= 5) { c.rx = 0; c.axisTs = now; recomputeAndSend(now); return; }
  if (strncmp(key, "*RY", 3) == 0 && strlen(key) <= 5) { c.ry = 0; c.axisTs = now; recomputeAndSend(now); return; }
}

// ====================================================================
// WebSocket
// ====================================================================
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
   if (type == WStype_TEXT) {
    anyWSActivity = true;
    const char* msg = (const char*)payload;

    if (strncmp(msg, "#HOST:", 6) == 0) {
      const char* h = msg + 6;
      if (h && strlen(h) > 0 && strlen(h) < 32) {
        snprintf(hostname, sizeof(hostname), "%s", h);
        Preferences p; p.begin("tgpadns", false); p.putString("hostname", hostname); p.end();
        webSocket.broadcastTXT(msg);
      }
    } else if (strncmp(msg, "#DZ:", 4) == 0) {
      int v = atoi(msg + 4);
      if (v >= 0 && v <= 64) {
        stickDeadzone = v;
        Preferences p; p.begin("tgpadns", false); p.putUChar("deadzone", v); p.end();
        webSocket.broadcastTXT(msg);
      }
    } else if (strncmp(msg, "#PING", 5) == 0) {
      if (num < MAX_WS_CLIENTS) clients[num].lastSeen = millis();
    } else if (length == 1 && msg[0] == '~') {
      handleKeyDown(num, msg, millis());
    } else if (length > 1 && msg[0] == '~') {
      handleKeyUp(num, msg + 1, millis());
    } else {
      handleKeyDown(num, msg, millis());
    }

  } else if (type == WStype_CONNECTED) {
    Serial.printf("[WS] Client %u connected\n", num);
    wsClientCount++;
    updateDisplay();
    clients[num] = ClientState();
    clients[num].active = true;
    anyWSActivity = true;
    webSocket.sendTXT(num, "Connected to TGPad-NS HID");
    char buf[40];
    snprintf(buf, sizeof(buf), "#HOST:%s", hostname);
    webSocket.sendTXT(num, buf);
    snprintf(buf, sizeof(buf), "#DZ:%d", stickDeadzone);
    webSocket.sendTXT(num, buf);
  } else if (type == WStype_DISCONNECTED) {
    Serial.printf("[WS] Client %u disconnected\n", num);
    if (wsClientCount > 0) wsClientCount--;
    updateDisplay();
    clients[num] = ClientState();
    recomputeAndSend(millis());
  }
}

#include "webpage.h"

void handleRoot() {
  server.send(200, "text/html", index_html);
}

// ====================================================================
// Display helpers (AtomS3 only)
// ====================================================================
#ifdef ARDUINO_M5STACK_ATOMS3
static bool initAtomS3Display() {
  static lgfx::Bus_SPI bus;
  static lgfx::Panel_GC9107 panel;

  auto busCfg = bus.config();
  busCfg.pin_mosi = GPIO_NUM_21;
  busCfg.pin_miso = (gpio_num_t)-1;
  busCfg.pin_sclk = GPIO_NUM_17;
  busCfg.pin_dc   = GPIO_NUM_33;
  busCfg.spi_mode = 0;
  busCfg.spi_3wire = true;
  busCfg.spi_host = SPI3_HOST;
  busCfg.freq_write = 40000000;
  busCfg.freq_read  = 16000000;
  bus.config(busCfg);
  bus.init();

  auto panelCfg = panel.config();
  panelCfg.pin_cs  = GPIO_NUM_15;
  panelCfg.pin_rst = GPIO_NUM_34;
  panelCfg.panel_width  = 128;
  panelCfg.panel_height = 128;
  panelCfg.offset_y = 32;
  panelCfg.readable = false;
  panelCfg.bus_shared = false;
  panel.config(panelCfg);
  panel.bus(&bus);

  static lgfx::Light_PWM light;
  auto lightCfg = light.config();
  lightCfg.pin_bl = GPIO_NUM_16;
  lightCfg.pwm_channel = 7;
  lightCfg.freq = 256;
  lightCfg.invert = false;
  lightCfg.offset = 48;
  light.config(lightCfg);
  light.init(128);
  panel.setLight(&light);

  display.init(&panel);
  display.setBrightness(128);
  return true;
}

static void bootMsg(const char* s1, const char* s2, const char* s3) {
  display.fillScreen(TFT_BLACK);
  display.setCursor(0, 0);
  display.setTextSize(2);
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.printf("TGPad-NS v%s", VERSION);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  int y = 18;
  if (s1) { display.setCursor(0, y); display.println(s1); y += 18; }
  if (s2) { display.setCursor(0, y); display.println(s2); y += 18; }
  if (s3) { display.setCursor(0, y); display.println(s3); }
}

static void updateDisplay() {
  display.fillScreen(TFT_BLACK);
  display.setCursor(0, 0);
  display.setTextSize(2);
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.printf("TGPad-NS v%s\n", VERSION);

  display.setTextColor(TFT_WHITE, TFT_BLACK);

  if (WiFi.status() == WL_CONNECTED) {
    display.println(WiFi.localIP());
    char buf[32];
    snprintf(buf, sizeof(buf), "%s.local", hostname);
    display.println(buf);
  } else {
    display.println("No WiFi");
  }
  display.printf("Clients: %d", wsClientCount);
}
#else
static void bootMsg(const char*, const char*, const char*) {}
static void updateDisplay() {}
#endif

void setup() {
  Serial.begin(115200);
  setCpuFrequencyMhz(240);
  delay(500);
  Serial.println("\n[INIT] Starting TGPad-NS...");

#ifdef ARDUINO_M5STACK_ATOMS3
  initAtomS3Display();
#endif
  bootMsg("Starting...", nullptr, nullptr);

  Gamepad.begin();
  
  // Explicitly initialize TinyUSB stack before WiFi/OTA to avoid conflicts
  USB.usbClass(0);
  USB.usbSubClass(0);
  USB.usbProtocol(0);
  if (!USB.begin()) { Serial.println("[ERR] USB init failed"); }

  {
    Preferences p;
    p.begin("tgpadns", true);
    stickDeadzone = p.getUChar("deadzone", 0);
    String h = p.getString("hostname", "tgpadns");
    snprintf(hostname, sizeof(hostname), "%s", h.c_str());
    p.end();
  }

  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);

  bootMsg("WiFi connecting...", nullptr, nullptr);
  WiFiManager wm;
  wm.setHostname(hostname);
  wm.addParameter(&customHostnameParam);
  wm.setSaveConfigCallback([]() { portalConfigSaved = true; });
  wm.setConfigPortalTimeout(180);
  wm.setConnectTimeout(20);
  if (!wm.autoConnect("TGPad-NS-Config")) {
    Serial.println("[WARN] WiFi timeout! Proceeding anyway.");
  }
  if (WiFi.status() == WL_CONNECTED) {
    if (portalConfigSaved) {
      const char* h = customHostnameParam.getValue();
      if (h && strlen(h) > 0) {
        snprintf(hostname, sizeof(hostname), "%s", h);
        Preferences p; p.begin("tgpadns", false); p.putString("hostname", hostname); p.end();
      }
    }

    char ipStr[16];
    snprintf(ipStr, sizeof(ipStr), "%d.%d.%d.%d",
             WiFi.localIP()[0], WiFi.localIP()[1],
             WiFi.localIP()[2], WiFi.localIP()[3]);

    char mdnsHostname[40];
    snprintf(mdnsHostname, sizeof(mdnsHostname), "%s.local", hostname);
    bootMsg(ipStr, mdnsHostname, nullptr);
    Serial.printf("[WiFi] Connected! IP=%s, hostname=%s\n", ipStr, hostname);
    esp_wifi_set_ps(WIFI_PS_NONE);
    Serial.println("[WiFi] Modem sleep disabled");
    ArduinoOTA.setHostname(hostname);
#ifdef OTA_PASS
    ArduinoOTA.setPassword(OTA_PASS);
    Serial.println("[OTA] Password enabled");
#endif
    ArduinoOTA.onStart([]() { Serial.println("[OTA] Start"); });
    ArduinoOTA.onEnd([]() { Serial.println("[OTA] End"); });
    ArduinoOTA.onProgress([](unsigned int p, unsigned int t) {
      Serial.printf("[OTA] Progress: %u%%\r", p * 100 / t);
    });
    ArduinoOTA.onError([](ota_error_t e) {
      Serial.printf("[OTA] Error: %u\n", e);
    });

    if (MDNS.begin(hostname)) {
      Serial.printf("[mDNS] Responder started at %s\n", mdnsHostname);
      MDNS.addService("http", "tcp", 80);
      MDNS.addService("ws", "tcp", 81);
      esp_netif_t *sta = WiFi.STA.netif();
      if (sta) {
        mdns_netif_action(sta, (mdns_event_actions_t)(MDNS_EVENT_ANNOUNCE_IP4 | MDNS_EVENT_ANNOUNCE_IP6));
        Serial.println("[mDNS] Announced on STA interface");
      }
    }

    ArduinoOTA.begin();
    Serial.println("[OTA] Ready");

    WiFi.onEvent([](arduino_event_id_t event, arduino_event_info_t info) {
      if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
        esp_netif_t *sta = WiFi.STA.netif();
        if (sta) {
          mdns_netif_action(sta, (mdns_event_actions_t)(MDNS_EVENT_ANNOUNCE_IP4 | MDNS_EVENT_ANNOUNCE_IP6));
          Serial.println("[mDNS] Re-announced on STA after GOT_IP");
        }
      }
    }, ARDUINO_EVENT_WIFI_STA_GOT_IP);
  } else {
    snprintf(hostname, sizeof(hostname), "tgpadns");
    bootMsg("WiFi failed!", nullptr, nullptr);
  }

  bootMsg("Starting server...", nullptr, nullptr);
  server.on("/", handleRoot);
  server.on("/favicon.ico", [](){server.send(204, "text/plain", "");});
  server.on("/update", HTTP_GET, []() {
#ifdef OTA_PASS
    if (!server.authenticate("admin", OTA_PASS)) return server.requestAuthentication(BASIC_AUTH, "TGPad-NS OTA");
#endif
    server.sendHeader("Connection", "close");
    server.send(200, "text/html",
      "<form method='POST' action='/update' enctype='multipart/form-data'>"
      "<input type='file' name='firmware'><br><br>"
      "<input type='submit' value='Update Firmware'>"
      "</form>");
  });
  server.on("/update", HTTP_POST, []() {
#ifdef OTA_PASS
    if (!server.authenticate("admin", OTA_PASS)) { server.send(401, "text/plain", "Unauthorized"); return; }
#endif
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", Update.hasError() ? "FAIL" : "OK");
    if (!Update.hasError()) delay(1000);
  }, []() {
    HTTPUpload &upload = server.upload();
    static bool uploadAborted = false;
    if (upload.status == UPLOAD_FILE_START) {
      uploadAborted = false;
#ifdef OTA_PASS
      if (!server.authenticate("admin", OTA_PASS)) { uploadAborted = true; return; }
#endif
      Serial.printf("[OTA Web] Start: %s (%u bytes)\n", upload.filename.c_str(), upload.totalSize);
      if (!Update.begin(upload.totalSize, U_FLASH)) {
        Update.printError(Serial);
        uploadAborted = true;
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (!uploadAborted && Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
      uploadAborted = false;
      Serial.println("[OTA Web] Upload aborted by client");
    } else if (upload.status == UPLOAD_FILE_END) {
      if (!uploadAborted) {
        if (Update.end(true)) {
          Serial.printf("[OTA Web] Success: %u bytes\n", upload.totalSize);
        } else {
          Update.printError(Serial);
        }
      }
    }
  });
  server.begin();
  Serial.println("[HTTP] WebServer started on port 80");

  webSocket.onEvent(webSocketEvent);
  webSocket.begin();
  Serial.println("[WS] WebSocketServer started on port 81");

  updateDisplay();
}

static void handleWdt(unsigned long now) {
  bool released = false;
  for (uint8_t i = 0; i < MAX_WS_CLIENTS; i++) {
    ClientState& c = clients[i];
    if (!c.active) continue;
    if (now - c.lastSeen > 5000) {
      bool held = false;
      for (uint8_t b = 0; b < 14; b++) if (c.btn[b]) held = true;
      if (held || c.axisTs || c.dpadTs) {
        Serial.printf("[WDT] Client %u silent >5s — releasing its inputs\n", i);
        c = ClientState();
        released = true;
      } else {
        c.lastSeen = now;
      }
    }
  }
  if (released) recomputeAndSend(now);
}

static void handleResetButton(unsigned long now) {
  bool resetPressed = digitalRead(RESET_BUTTON_PIN) == LOW;
  if (resetPressed && !resetButtonWasLow) {
    resetPressStart = now;
    resetButtonWasLow = true;
  } else if (resetPressed && resetButtonWasLow) {
    if (now - resetPressStart >= 5000) {
      Serial.println("[WiFi] Button held 5s — erasing credentials and rebooting");
#ifdef ARDUINO_M5STACK_ATOMS3
      bootMsg("Resetting", "WiFi...", nullptr);
#endif
      delay(100);
      WiFiManager wm;
      wm.resetSettings();
      delay(500);
      ESP.restart();
    }
  } else {
    resetButtonWasLow = false;
  }
}

void loop() {
  webSocket.loop();
  server.handleClient();
  ArduinoOTA.handle();
  unsigned long now = millis();
  handleWdt(now);
  handleResetButton(now);
}
