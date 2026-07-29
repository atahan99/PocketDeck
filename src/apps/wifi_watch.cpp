#include <WiFi.h>
#include <esp_wifi.h>
#include <cstring>

#include "apps.h"
#include "ui.h"

using ui::fb;

// Passive 802.11 watcher -- promiscuous-mode sniff that flags two attack classes
// while transmitting nothing. It is the receive-only counterpart to the spam
// tools: you can see the attacks you can also demo.
//   Deauth Hunter -- counts deauth/disassoc management frames. A burst means
//     someone nearby is kicking clients off an AP (or running a deauther).
//   PineAP / Karma -- one source MAC beaconing many different SSIDs is the
//     signature of a WiFi Pineapple impersonating every network it overhears.
// Channel-hops 1..13 so it covers the whole 2.4GHz band. BtnA resets counters.
//
// ponytail: the sniff cb and the UI read the same counters without a lock. It's
// a display, not accounting -- a torn read costs one stale frame, not a bug.

namespace {

volatile uint32_t g_deauth = 0;      // running totals (written from the WiFi task)
volatile uint32_t g_beacon = 0;
volatile uint32_t g_deauth_win = 0;  // deauths this 1s window
volatile int g_pineap = 0;           // worst distinct-SSID count seen from one BSSID

struct ApRec { uint8_t bssid[6]; uint8_t hashes[12]; uint8_t n; };
ApRec g_aps[24];
int g_nap = 0;

int find_ap(const uint8_t *b) {
  for (int i = 0; i < g_nap; i++)
    if (!memcmp(g_aps[i].bssid, b, 6)) return i;
  if (g_nap < (int)(sizeof(g_aps) / sizeof(g_aps[0]))) {
    memcpy(g_aps[g_nap].bssid, b, 6);
    g_aps[g_nap].n = 0;
    return g_nap++;
  }
  return -1;   // table full; stop tracking new APs
}

void sniff(void *buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT) return;
  auto *pkt = (wifi_promiscuous_pkt_t *)buf;
  const uint8_t *p = pkt->payload;
  uint8_t subtype = (p[0] >> 4) & 0x0F;         // frame-control subtype

  if (subtype == 0x0C || subtype == 0x0A) {     // deauth / disassoc
    g_deauth++;
    g_deauth_win++;
    return;
  }
  if (subtype == 0x08 || subtype == 0x05) {     // beacon / probe-response
    g_beacon++;
    if (pkt->rx_ctrl.sig_len < 38) return;      // too short to hold an SSID tag
    const uint8_t *bssid = p + 10;              // addr2 == the AP's MAC
    const uint8_t *tag = p + 36;                // SSID tag: 24 hdr + 12 fixed
    if (tag[0] != 0 || tag[1] == 0 || tag[1] > 32) return;
    if (36 + 2 + tag[1] > pkt->rx_ctrl.sig_len) return;
    uint8_t h = tag[1];                          // cheap hash = len + byte-sum
    for (int i = 0; i < tag[1]; i++) h += tag[2 + i];
    int a = find_ap(bssid);
    if (a < 0) return;
    ApRec &r = g_aps[a];
    for (int i = 0; i < r.n; i++) if (r.hashes[i] == h) return;   // SSID already seen
    if (r.n < (int)sizeof(r.hashes)) r.hashes[r.n++] = h;
    if (r.n > g_pineap) g_pineap = r.n;
  }
}

void reset() { g_deauth = g_beacon = g_deauth_win = 0; g_pineap = 0; g_nap = 0; }

}  // namespace

void app_wifi_watch() {
  bool was_on = ui::wifi_on();
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, true);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&sniff);
  reset();

  uint8_t ch = 1;
  uint32_t last_hop = 0, last_sec = millis(), deauth_rate = 0;

  while (true) {
    uint32_t now = millis();
    if (now - last_hop > 250) {                 // hop to cover the band
      ch = ch % 13 + 1;
      esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
      last_hop = now;
    }
    if (now - last_sec >= 1000) {
      deauth_rate = g_deauth_win; g_deauth_win = 0; last_sec = now;
    }

    bool alert = deauth_rate >= 5 || g_pineap >= 6;
    ui::header("WiFi Watch", alert ? TFT_RED : TFT_ORANGE);
    fb.setTextSize(1);
    fb.setTextColor(TFT_DARKGREY, TFT_BLACK);
    fb.setCursor(150, 4);
    fb.printf("ch %2d", ch);

    fb.setTextSize(2);
    fb.setTextColor(deauth_rate ? TFT_RED : TFT_WHITE, TFT_BLACK);
    fb.setCursor(4, 22);
    fb.printf("Deauth %lu", (unsigned long)g_deauth);
    fb.setTextSize(1);
    fb.setTextColor(TFT_DARKGREY, TFT_BLACK);
    fb.setCursor(4, 42);
    fb.printf("now %lu/s", (unsigned long)deauth_rate);

    fb.setTextSize(2);
    fb.setTextColor(g_pineap >= 6 ? TFT_RED : TFT_WHITE, TFT_BLACK);
    fb.setCursor(4, 58);
    fb.printf("PineAP %d", g_pineap);
    fb.setTextSize(1);
    fb.setTextColor(TFT_DARKGREY, TFT_BLACK);
    fb.setCursor(4, 78);
    fb.printf("max SSIDs/AP  beacons %lu", (unsigned long)g_beacon);

    fb.setTextColor(alert ? TFT_RED : TFT_DARKGREY, TFT_BLACK);
    fb.setCursor(4, 100);
    fb.print(alert ? "!! ATTACK LIKELY  A=reset" : "watching...  A=reset");

    ui::stay_awake();
    ui::Ev ev = ui::frame(0);
    if (ev.home) break;
    if (ev.a) reset();
  }

  esp_wifi_set_promiscuous(false);
  ui::set_wifi_on(was_on);
}
