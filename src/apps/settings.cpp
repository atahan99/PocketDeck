#include <WiFi.h>
#include <WiFiManager.h>

#include "apps.h"
#include "ui.h"

using ui::fb;

// Settings: on-device WiFi provisioning (no baked creds -> the firmware ships
// as-is), display brightness, and an about screen.
//
// WiFi Setup runs a captive portal: the device becomes an AP "StickS3-Setup",
// you join it from a phone and enter your network. WiFiManager persists the
// creds in NVS, so WiFi.begin() reconnects on later boots (AI Buddy WiFi mode
// relies on that).

namespace {
void wifi_setup() {
  // Static instruction screen -- the portal blocks, so no live frame() during it.
  fb.fillScreen(TFT_BLACK);
  ui::header("WiFi Setup", TFT_CYAN);
  fb.setTextColor(TFT_WHITE, TFT_BLACK);
  fb.setCursor(4, 24);
  fb.print("1. Join WiFi:");
  fb.setTextColor(TFT_GREEN, TFT_BLACK);
  fb.setCursor(4, 40);
  fb.print("   StickS3-Setup");
  fb.setTextColor(TFT_WHITE, TFT_BLACK);
  fb.setCursor(4, 60);
  fb.print("2. Pick your network");
  fb.setCursor(4, 76);
  fb.print("3. 3 min timeout");
  fb.pushSprite(0, 0);

  WiFiManager wm;
  wm.setConfigPortalTimeout(180);
  bool ok = wm.startConfigPortal("StickS3-Setup");
  ui::toast(ok ? "WiFi saved" : "portal timed out");
  delay(1200);
}

// WiFi hub: radio is off by default. A = toggle on/off, B = captive-portal setup.
void wifi_page() {
  while (true) {
    ui::header("WiFi", TFT_CYAN);
    bool on = ui::wifi_on();
    wl_status_t st = WiFi.status();
    fb.setTextSize(2);
    fb.setTextColor(on ? TFT_GREEN : TFT_DARKGREY, TFT_BLACK);
    fb.setCursor(6, 24);
    fb.printf("Radio: %s", on ? "ON" : "OFF");
    fb.setTextSize(1);
    fb.setTextColor(TFT_WHITE, TFT_BLACK);
    fb.setCursor(6, 52);
    if (!on) fb.print("(not broadcasting)");
    else if (st == WL_CONNECTED) { fb.printf("%s", WiFi.SSID().c_str()); fb.setCursor(6, 66); fb.printf("%s", WiFi.localIP().toString().c_str()); }
    else fb.print("connecting...");
    fb.setTextColor(TFT_DARKGREY, TFT_BLACK);
    fb.setCursor(6, ui::H - 10);
    fb.print("A=toggle  B=setup network");

    ui::Ev ev = ui::frame(30);   // responsive long-press for back
    if (ev.home) return;
    if (ev.a) ui::set_wifi_on(!on);
    if (ev.b) wifi_setup();
  }
}

void brightness() {
  int b = ui::user_brightness;
  while (true) {
    ui::header("Brightness", TFT_CYAN);
    fb.fillRect(20, 60, 200, 16, TFT_DARKGREY);
    fb.fillRect(20, 60, 200 * b / 255, 16, TFT_CYAN);
    fb.setTextColor(TFT_WHITE, TFT_BLACK);
    fb.setCursor(100, 90);
    fb.printf("%d", b);
    ui::Ev ev = ui::frame();
    if (ev.home) return;
    if (ev.a) b = max(15, b - 25);
    if (ev.b) b = min(255, b + 25);
    ui::set_brightness(b);   // apply + persist to NVS
  }
}

void about() {
  while (true) {
    ui::header("About", TFT_CYAN);
    fb.setTextColor(TFT_WHITE, TFT_BLACK);
    fb.setCursor(4, 26);
    fb.print("StickS3 Launcher");
    fb.setTextColor(TFT_DARKGREY, TFT_BLACK);
    fb.setCursor(4, 44);
    fb.printf("board id %d", (int)M5.getBoard());
    fb.setCursor(4, 60);
    if (WiFi.isConnected()) fb.printf("wifi: %s", WiFi.SSID().c_str());
    else fb.print("wifi: not connected");
    fb.setCursor(4, 76);
    fb.printf("IP: %s", WiFi.isConnected() ? WiFi.localIP().toString().c_str() : "-");
    if (ui::frame().home) return;
  }
}
}  // namespace

void app_settings() {
  int sel = 0;
  while (true) {
    char grove[20];
    snprintf(grove, sizeof(grove), "Grove 5V: %s", ui::grove_5v() ? "ON" : "off");
    const char *items[] = {"WiFi", "Brightness", grove, "About"};
    int r = ui::menu("Settings", TFT_CYAN, items, 4, sel);
    if (r == ui::MENU_BACK) return;
    if (r == 0) wifi_page();
    else if (r == 1) brightness();
    else if (r == 2) ui::set_grove_5v(!ui::grove_5v());   // re-enable if you attach a 5V Grove unit
    else if (r == 3) about();
  }
}
