#include <NimBLEDevice.h>

#include "apps.h"
#include "ui.h"

using ui::fb;

// Passive BLE spam detector -- the receive-side counterpart to BLE Spam. Watches
// advertisements for the manufacturer IDs that pairing-spam tools abuse (Apple
// Continuity, Microsoft Swift Pair, Samsung) plus Google Fast Pair service data,
// and alarms when they flood in. Each spam burst rotates its MAC, so a high rate
// of these ads is the tell. Receive-only; it transmits nothing.

namespace {

volatile uint32_t g_apple = 0, g_ms = 0, g_samsung = 0, g_google = 0;  // cumulative
// Per-vendor counts in the current 1s window. Measured on real hardware, ambient
// phones show up as Apple-only (Windows/Google stay flat 0); a Flipper "all"-mode
// spam lights up 3-4 vendors at once at ~40/s. So we alarm on EITHER a high total
// rate OR 3+ vendors active -- the latter is what a room full of iPhones can't fake.
volatile uint32_t g_aw = 0, g_mw = 0, g_sw = 0, g_gw = 0;

class CB : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice *d) override {
    if (d->haveManufacturerData()) {
      std::string m = d->getManufacturerData();
      if (m.size() >= 2) {
        uint16_t id = (uint8_t)m[0] | ((uint8_t)m[1] << 8);
        if (id == 0x004C) { g_apple++; g_aw++; }
        else if (id == 0x0006) { g_ms++; g_mw++; }
        else if (id == 0x0075) { g_samsung++; g_sw++; }
      }
    }
    if (d->isAdvertisingService(NimBLEUUID((uint16_t)0xFE2C))) { g_google++; g_gw++; }
  }
};

}  // namespace

void app_ble_hunter() {
  NimBLEDevice::init("");
  NimBLEScan *scan = NimBLEDevice::getScan();
  static CB cb;
  scan->setScanCallbacks(&cb, true);   // wantDuplicates: count every burst
  scan->setActiveScan(true);
  scan->setInterval(45);
  scan->setWindow(45);
  scan->start(0, false);               // continuous, non-blocking

  g_apple = g_ms = g_samsung = g_google = 0;
  g_aw = g_mw = g_sw = g_gw = 0;
  uint32_t last = millis(), rate = 0;
  int vendors = 0;

  while (true) {
    if (millis() - last >= 1000) {
      rate = g_aw + g_mw + g_sw + g_gw;
      vendors = (g_aw > 0) + (g_mw > 0) + (g_sw > 0) + (g_gw > 0);
      g_aw = g_mw = g_sw = g_gw = 0;
      last = millis();
    }
    bool alert = rate >= 30 || vendors >= 3;   // rate OR multi-vendor (see note above)

    ui::header("BLE Hunter", alert ? TFT_RED : TFT_BLUE);
    fb.setTextSize(2);
    fb.setTextColor(alert ? TFT_RED : TFT_WHITE, TFT_BLACK);
    fb.setCursor(6, 20);
    fb.printf("%lu/s", (unsigned long)rate);
    fb.setTextSize(1);
    fb.setTextColor(alert ? TFT_RED : TFT_DARKGREY, TFT_BLACK);
    fb.setCursor(90, 20);
    fb.print(alert ? "!! SPAM FLOOD" : "spam ads/sec");
    fb.setCursor(90, 32);
    fb.printf("%d vendors", vendors);

    fb.setTextColor(TFT_WHITE, TFT_BLACK);
    fb.setCursor(6, 52);
    fb.printf("Apple   %lu", (unsigned long)g_apple);
    fb.setCursor(6, 66);
    fb.printf("Windows %lu", (unsigned long)g_ms);
    fb.setCursor(6, 80);
    fb.printf("Samsung %lu", (unsigned long)g_samsung);
    fb.setCursor(6, 94);
    fb.printf("Google  %lu", (unsigned long)g_google);
    fb.setTextColor(TFT_DARKGREY, TFT_BLACK);
    fb.setCursor(6, 112);
    fb.print("A=reset  hold=exit");

    ui::stay_awake();
    ui::Ev ev = ui::frame(0);
    if (ev.home) break;
    if (ev.a) { g_apple = g_ms = g_samsung = g_google = 0; g_aw = g_mw = g_sw = g_gw = 0; }
  }

  scan->stop();
  NimBLEDevice::deinit(true);
}
