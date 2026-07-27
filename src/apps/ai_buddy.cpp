#include <ArduinoJson.h>
#include <NimBLEDevice.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#include "apps.h"
#include "ui.h"

using ui::fb;

// AI coding companion. Shows Claude Code / Codex / Cursor session status pushed
// from a host running scripts/buddy.py. Two selectable transports:
//   WiFi -- buddy.py UDP-broadcasts JSON to port 5533 (device uses saved creds)
//   BLE  -- buddy.py writes JSON to a GATT characteristic
// Display-only. ponytail: state is idle/running from log-file recency; reliable
// "awaiting approval" detection isn't generic across the three CLIs, so it's not
// claimed here.

namespace {
constexpr uint16_t UDP_PORT = 5533;
const char *SVC_UUID = "0000a000-0000-1000-8000-00805f9b34fb";
const char *CHR_UUID = "0000a001-0000-1000-8000-00805f9b34fb";

struct Agent { char name[12]; int state; char task[44]; };
Agent g_agents[3];
int g_n = 0;
uint32_t g_last_rx = 0;

char g_ble_buf[512];
volatile bool g_ble_pending = false;

void parse_json(const char *s, size_t len) {
  JsonDocument doc;
  if (deserializeJson(doc, s, len)) return;
  JsonArray arr = doc["agents"];
  g_n = 0;
  for (JsonObject a : arr) {
    if (g_n >= 3) break;
    strlcpy(g_agents[g_n].name, a["n"] | "?", sizeof(g_agents[0].name));
    g_agents[g_n].state = a["s"] | 0;
    strlcpy(g_agents[g_n].task, a["t"] | "", sizeof(g_agents[0].task));
    g_n++;
  }
  g_last_rx = millis();
}

uint16_t state_color(int s) {
  return s == 2 ? TFT_RED : s == 1 ? TFT_YELLOW : TFT_GREEN;   // attn / running / idle
}
const char *state_word(int s) { return s == 2 ? "attn" : s == 1 ? "run" : "idle"; }

void render(const char *mode, const char *conn) {
  ui::header("AI Buddy", TFT_GREEN);
  fb.setTextColor(TFT_DARKGREY, TFT_BLACK);
  fb.setCursor(150, 4);
  fb.print(mode);

  bool fresh = g_n > 0 && millis() - g_last_rx < 8000;
  if (!fresh) {
    fb.setTextColor(TFT_WHITE, TFT_BLACK);
    fb.setCursor(6, 40);
    fb.print(conn);
    fb.setTextColor(TFT_DARKGREY, TFT_BLACK);
    fb.setCursor(6, 58);
    fb.print("run scripts/buddy.py");
    return;
  }
  for (int i = 0; i < g_n; i++) {
    int y = 20 + i * 36;
    fb.fillCircle(12, y + 10, 6, state_color(g_agents[i].state));
    fb.setTextColor(TFT_WHITE, TFT_BLACK);
    fb.setTextSize(2);
    fb.setCursor(26, y);
    fb.print(g_agents[i].name);
    fb.setTextSize(1);
    fb.setTextColor(state_color(g_agents[i].state), TFT_BLACK);
    fb.setCursor(150, y + 2);
    fb.print(state_word(g_agents[i].state));
    fb.setTextColor(TFT_DARKGREY, TFT_BLACK);
    fb.setCursor(26, y + 18);
    fb.print(g_agents[i].task);
  }
}

class BleCb : public NimBLECharacteristicCallbacks {
  // NimBLE 2.x signature (adds NimBLEConnInfo). 1.4.x hangs on ESP-IDF 5.5.
  void onWrite(NimBLECharacteristic *c, NimBLEConnInfo &) override {
    NimBLEAttValue v = c->getValue();
    size_t n = min((size_t)v.length(), sizeof(g_ble_buf) - 1);
    memcpy(g_ble_buf, v.data(), n);
    g_ble_buf[n] = 0;
    g_ble_pending = true;
  }
};

void run_wifi() {
  g_n = 0;
  WiFi.mode(WIFI_STA);
  WiFi.begin();                       // saved creds from WiFiManager
  WiFiUDP udp;
  bool up = false;
  char buf[512];
  while (true) {
    if (!up && WiFi.status() == WL_CONNECTED) { udp.begin(UDP_PORT); up = true; }
    if (up) {
      int sz = udp.parsePacket();
      if (sz > 0) { int n = udp.read(buf, sizeof(buf) - 1); if (n > 0) parse_json(buf, n); }
    }
    const char *conn = WiFi.status() == WL_CONNECTED ? "waiting for buddy.py"
                                                     : "no WiFi (see Settings)";
    render("WiFi", conn);
    if (ui::frame(20).home) break;
  }
  WiFi.mode(WIFI_OFF);
}

void run_ble() {
  g_n = 0;
  g_ble_pending = false;
  NimBLEDevice::init("StickS3-Buddy");
  NimBLEServer *srv = NimBLEDevice::createServer();
  NimBLEService *svc = srv->createService(SVC_UUID);
  NimBLECharacteristic *chr = svc->createCharacteristic(CHR_UUID, NIMBLE_PROPERTY::WRITE);
  static BleCb cb;
  chr->setCallbacks(&cb);
  // NimBLE 2.x auto-starts services when the server starts; no svc->start().
  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(SVC_UUID);
  adv->start();

  while (true) {
    if (g_ble_pending) { g_ble_pending = false; parse_json(g_ble_buf, strlen(g_ble_buf)); }
    render("BLE", "advertising StickS3-Buddy");
    if (ui::frame(20).home) break;
  }
  NimBLEDevice::deinit(true);
}
}  // namespace

void app_ai_buddy() {
  const char *modes[] = {"WiFi", "BLE"};
  int sel = 0;
  while (true) {
    ui::header("AI Buddy", TFT_GREEN);
    fb.setTextColor(TFT_WHITE, TFT_BLACK);
    fb.setCursor(6, 22);
    fb.print("Transport:");
    fb.setTextSize(2);
    for (int i = 0; i < 2; i++) {
      int y = 44 + i * 32;
      bool on = i == sel;
      if (on) fb.fillRoundRect(4, y - 2, ui::W - 8, 28, 4, TFT_DARKGREEN);
      fb.setTextColor(on ? TFT_WHITE : TFT_GREEN, on ? TFT_DARKGREEN : TFT_BLACK);
      fb.setCursor(14, y + 4);
      fb.print(modes[i]);
    }
    fb.setTextSize(1);
    ui::Ev ev = ui::frame();
    if (ev.home) return;
    if (ev.b) sel = (sel + 1) % 2;
    if (ev.a) { if (sel == 0) run_wifi(); else run_ble(); }
  }
}
