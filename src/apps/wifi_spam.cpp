#include <WiFi.h>
#include <esp_wifi.h>
#include <cstring>

#include "apps.h"
#include "ui.h"

using ui::fb;

// Raw 802.11 beacon flooder -- broadcasts fake APs so nearby devices' Wi-Fi
// lists fill with funny/junk SSIDs. Nuisance-class, same warning gate as BLE
// Spam: it affects every Wi-Fi scanner in range, so it is for authorized /
// your-own-devices testing only. It ONLY transmits beacons -- no deauth, no
// association, it never touches real networks.
//
// ponytail: fixed funny list + random names, fresh random MAC per frame. No
// PMKID capture, no client handling -- a beacon spammer, not an AP.

namespace {

const char *FUNNY[] = {
    "FBI Surveillance Van", "Pretty Fly for a WiFi", "Loading...",
    "Mom Use This One", "Virus.exe", "Hide Yo Kids Hide Yo WiFi",
    "The LAN Before Time", "Drop It Like Its Hotspot",
    "404 Network Unavailable", "Tell My WiFi Love Her",
};
const int NF = sizeof(FUNNY) / sizeof(FUNNY[0]);

// Beacon frame up to the SSID tag; rates + DS-param get appended per send.
uint8_t TMPL[] = {
    0x80, 0x00, 0x00, 0x00,                          // type=beacon, duration
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff,              // dest = broadcast
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              // src   (randomized)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              // bssid (randomized)
    0x00, 0x00,                                      // seq
    0x83, 0x51, 0xf7, 0x8f, 0x0f, 0x00, 0x00, 0x00,  // timestamp
    0x64, 0x00,                                      // beacon interval
    0x01, 0x04,                                      // capability (ESS)
    0x00, 0x00,                                      // SSID tag id + len (filled)
};

const char *pick_ssid(int mode, char *buf) {
  bool rnd = (mode == 1) || (mode == 2 && random(2));   // 0=funny 1=random 2=both
  if (!rnd) return FUNNY[random(NF)];
  static const char cs[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  int len = 6 + random(6);
  for (int i = 0; i < len; i++) buf[i] = cs[random(62)];
  buf[len] = 0;
  return buf;
}

void send_beacon(const char *ssid, uint8_t ch) {
  uint8_t pkt[128];
  int slen = strlen(ssid);
  if (slen > 32) slen = 32;
  memcpy(pkt, TMPL, sizeof(TMPL));
  for (int i = 10; i < 16; i++) pkt[i] = random(256);   // random source MAC
  for (int i = 16; i < 22; i++) pkt[i] = pkt[i - 6];    // bssid = source
  pkt[37] = slen;
  memcpy(pkt + 38, ssid, slen);
  int o = 38 + slen;
  uint8_t rates[] = {0x01, 0x08, 0x82, 0x84, 0x8b, 0x96, 0x24, 0x30, 0x48, 0x6c};
  memcpy(pkt + o, rates, sizeof(rates));
  o += sizeof(rates);
  pkt[o++] = 0x03; pkt[o++] = 0x01; pkt[o++] = ch;      // DS param (channel)
  esp_wifi_80211_tx(WIFI_IF_STA, pkt, o, false);
}

bool warn_gate() {
  while (true) {
    fb.fillScreen(TFT_BLACK);
    ui::header("WiFi Spam", TFT_RED);
    fb.setTextColor(TFT_YELLOW, TFT_BLACK);
    fb.setCursor(4, 24);
    fb.print("Floods fake APs to");
    fb.setCursor(4, 38);
    fb.print("every scanner near.");
    fb.setTextColor(TFT_WHITE, TFT_BLACK);
    fb.setCursor(4, 58);
    fb.print("Authorized/own use");
    fb.setCursor(4, 72);
    fb.print("only. A=ok  hold=no");
    ui::Ev ev = ui::frame();
    if (ev.home) return false;
    if (ev.a) return true;
  }
}

void spam_run(int mode) {
  const char *MNAME[] = {"Funny", "Random", "Both"};
  uint32_t sent = 0;
  uint8_t ch = 1;
  uint32_t last_hop = 0;
  char buf[16];
  while (true) {
    ui::stay_awake();
    uint32_t now = millis();
    if (now - last_hop > 500) {
      ch = ch % 13 + 1;
      esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
      last_hop = now;
    }
    for (int i = 0; i < 20; i++) {                 // a batch per frame
      send_beacon(pick_ssid(mode, buf), ch);
      sent++;
    }

    ui::header("WiFi Spam", TFT_RED);
    fb.setTextSize(2);
    fb.setTextColor(TFT_WHITE, TFT_BLACK);
    fb.setCursor(6, 40);
    fb.print(MNAME[mode]);
    fb.setTextSize(1);
    fb.setTextColor(TFT_DARKGREY, TFT_BLACK);
    fb.setCursor(6, 70);
    fb.printf("beacons: %lu  ch %d", (unsigned long)sent, ch);
    fb.setCursor(6, 100);
    fb.print("long-press = stop");

    if (ui::frame(0).home) break;
  }
}

}  // namespace

void app_wifi_spam() {
  if (!warn_gate()) return;
  bool was_on = ui::wifi_on();
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, true);
  esp_wifi_set_promiscuous(true);   // lets us tx on any channel unassociated

  const char *items[] = {"Funny SSIDs", "Random", "Both"};
  int sel = 0;
  while (true) {
    int r = ui::menu("WiFi Spam", TFT_RED, items, 3, sel);
    if (r == ui::MENU_BACK) break;
    if (r >= 0) spam_run(r);
  }

  esp_wifi_set_promiscuous(false);
  ui::set_wifi_on(was_on);
}
