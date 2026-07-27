#include <WiFi.h>
#include <esp_wifi.h>

#include "apps.h"
#include "ui.h"

using ui::fb;

// 2.4GHz channel activity monitor. Promiscuous sniff, hop channels 1..13, count
// frames per channel into decaying bars, and show the last frame's RSSI / channel
// / privacy in a HUD. Matrix green.
//
// ponytail: "encryption" is the beacon Privacy capability bit only (OPEN vs ENC),
// not the full WPA2/WPA3 breakdown -- that needs RSN-IE parsing. Bar height is a
// frame count, not calibrated dBm energy; a real SDR this is not.

namespace {
constexpr int CH = 13;
volatile uint16_t g_count[CH + 1];      // per-channel frame counter (1..13)
volatile int8_t g_last_rssi = 0;
volatile uint8_t g_last_ch = 0;
volatile bool g_last_enc = false;

void sniff(void *buf, wifi_promiscuous_pkt_type_t type) {
  auto *p = (wifi_promiscuous_pkt_t *)buf;
  int ch = p->rx_ctrl.channel;
  if (ch >= 1 && ch <= CH) { uint16_t v = g_count[ch]; g_count[ch] = v + 1; }  // volatile: no compound op
  g_last_rssi = p->rx_ctrl.rssi;
  g_last_ch = ch;
  if (type == WIFI_PKT_MGMT && p->rx_ctrl.sig_len >= 36) {
    uint8_t fc = p->payload[0];           // beacon=0x80, probe-resp=0x50
    if (fc == 0x80 || fc == 0x50) g_last_enc = p->payload[34] & 0x10;  // Privacy bit
  }
}
}  // namespace

void app_spectrum() {
  WiFi.mode(WIFI_STA);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&sniff);
  for (int i = 0; i <= CH; i++) g_count[i] = 0;

  float bar[CH + 1] = {0};
  int ch = 1;
  uint32_t hop_at = 0;

  while (true) {
    uint32_t now = millis();
    if (now >= hop_at) {                  // end of this channel's dwell
      bar[ch] = bar[ch] * 0.3f + g_count[ch] * 0.7f;  // fold full dwell count
      g_count[ch] = 0;
      ch = ch % CH + 1;
      esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
      hop_at = now + 110;
    }

    ui::stay_awake();   // user watches without pressing keys; don't dim/sleep
    ui::header("Spectrum 2.4G", TFT_GREEN);
    const int base = 100, top = 18;   // landscape 240x135; bottom ~35px = labels + HUD
    const int bw = ui::W / CH;             // ~18px per channel
    for (int c = 1; c <= CH; c++) {
      int h = (int)(bar[c] * 5);           // frames-per-dwell -> pixels
      if (h > base - top) h = base - top;
      int x = (c - 1) * bw;
      uint16_t col = (c == ch) ? TFT_WHITE : TFT_GREEN;
      fb.fillRect(x + 1, base - h, bw - 1, h, col);
      fb.setTextColor(TFT_DARKGREEN, TFT_BLACK);
      fb.setCursor(x + 1, base + 2);
      fb.printf("%d", c % 10);             // single-digit channel label
    }

    // Live target HUD
    fb.drawFastHLine(0, base + 12, ui::W, TFT_DARKGREEN);
    fb.setTextColor(TFT_GREEN, TFT_BLACK);
    fb.setCursor(2, base + 15);
    if (g_last_ch)
      fb.printf("RSSI %d  CH %d  %s", (int)g_last_rssi, (int)g_last_ch,
                g_last_enc ? "ENC" : "OPEN");
    else
      fb.print("listening...");

    if (ui::frame(20).home) break;         // ~50fps; bars only move on 110ms hops
  }

  esp_wifi_set_promiscuous(false);
  WiFi.mode(WIFI_OFF);
}
