#include <WiFi.h>

#include "apps.h"
#include "ui.h"

using ui::fb;

// 2.4GHz AP scanner. BtnA rescans, BtnB scrolls, long-press exits.
// Line: "<ch> <rssi> <ssid> [*]"  -- * = encrypted. Color tracks RSSI.

static uint16_t rssi_color(int32_t r) {
  if (r >= -55) return TFT_GREEN;
  if (r >= -70) return TFT_YELLOW;
  if (r >= -80) return TFT_ORANGE;
  return TFT_RED;
}

void app_wifi_scan() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, true);

  int n = -1;          // -1 = scanning
  int top = 0;         // first visible row
  const int rows = 7;  // landscape 240x135 fits ~7 rows

  auto rescan = [&]() {
    ui::toast("Scanning...");
    n = WiFi.scanNetworks(false, true);   // blocking, include hidden
    top = 0;
  };
  rescan();

  while (true) {
    ui::header("WiFi Scan", TFT_CYAN);
    fb.setTextSize(1);
    if (n <= 0) {
      fb.setTextColor(TFT_WHITE, TFT_BLACK);
      fb.setCursor(4, 110);
      fb.print(n == 0 ? "no networks" : "scan failed");
    } else {
      fb.setCursor(180, 4);
      fb.setTextColor(TFT_DARKGREY, TFT_BLACK);
      fb.printf("%d found", n);
      for (int i = 0; i < rows && top + i < n; i++) {
        int idx = top + i;
        int y = 20 + i * 16;
        bool enc = WiFi.encryptionType(idx) != WIFI_AUTH_OPEN;
        fb.setTextColor(rssi_color(WiFi.RSSI(idx)), TFT_BLACK);
        fb.setCursor(2, y);
        String ss = WiFi.SSID(idx);
        if (ss.isEmpty()) ss = "<hidden>";
        if (ss.length() > 26) ss = ss.substring(0, 26);   // wide landscape row
        fb.printf("%2d %4d %s%s", WiFi.channel(idx), (int)WiFi.RSSI(idx),
                  ss.c_str(), enc ? " *" : "");
      }
    }

    ui::Ev ev = ui::frame();
    if (ev.home) break;
    if (ev.a) rescan();
    if (ev.b && n > rows) top = (top + rows) % n;   // page down, wraps
  }
  WiFi.mode(WIFI_OFF);
}
