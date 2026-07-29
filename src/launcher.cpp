#include "apps.h"
#include "ui.h"

using ui::fb;

// Registry. color/icon drive the carousel; icon id == index here, matched in
// draw_icon(). Order = carousel order.
const App APPS[] = {
    {"WiFi Scan", app_wifi_scan, TFT_CYAN, 0},
    {"WiFi Watch", app_wifi_watch, TFT_ORANGE, 11},
    {"WiFi Spam", app_wifi_spam, TFT_RED, 12},
    {"Spectrum", app_spectrum, TFT_GREEN, 1},
    {"Audio Viz", app_audio_viz, TFT_MAGENTA, 2},
    {"IR Tools", app_ir_tools, TFT_ORANGE, 3},
    {"AI Buddy", app_ai_buddy, TFT_GREENYELLOW, 4},
    {"BLE Scan", app_ble_scan, TFT_BLUE, 7},
    {"BLE Spam", app_ble_spam, TFT_RED, 8},
    {"BLE Hunter", app_ble_hunter, TFT_GREEN, 13},
    {"Time", app_clock, TFT_CYAN, 9},
    {"Light", app_flashlight, TFT_WHITE, 10},
    {"Toys", app_toys, TFT_YELLOW, 5},
    {"Settings", app_settings, TFT_CYAN, 6},
};
const int APP_COUNT = sizeof(APPS) / sizeof(APPS[0]);

// Simple vector icons drawn from primitives -- no bitmap assets. cx,cy = center,
// s = half-size. Kept recognizable rather than pretty.
static void draw_icon(uint8_t id, int cx, int cy, int s, uint16_t col) {
  switch (id) {
    case 0:  // WiFi: concentric arcs opening up + dot
      for (int r = s; r > 0; r -= s / 3)
        fb.fillArc(cx, cy + s / 2, r, r - 2, 200, 340, col);
      fb.fillCircle(cx, cy + s / 2, 2, col);
      break;
    case 1:  // Spectrum: ascending bars
      for (int i = 0; i < 4; i++)
        fb.fillRect(cx - s + i * (s / 2), cy + s - (i + 1) * (s / 2), s / 2 - 2, (i + 1) * (s / 2), col);
      break;
    case 2:  // Audio: speaker cone + sound arcs
      fb.fillRect(cx - s, cy - s / 3, s / 2, 2 * s / 3, col);
      fb.fillTriangle(cx - s / 2, cy - s / 2, cx - s / 2, cy + s / 2, cx, cy, col);
      fb.fillTriangle(cx - s / 2, cy, cx, cy - s / 2, cx, cy + s / 2, col);
      fb.drawArc(cx, cy, s, s - 2, 300, 60, col);
      fb.drawArc(cx + s / 3, cy, s / 2, s / 2 - 2, 300, 60, col);
      break;
    case 3:  // IR: emitter + diverging beams
      fb.fillCircle(cx, cy + s, 3, col);
      for (int a = -1; a <= 1; a++)
        fb.drawLine(cx, cy + s, cx + a * s, cy - s, col);
      break;
    case 4:  // AI: robot head + eyes + antenna
      fb.drawLine(cx, cy - s, cx, cy - s - 4, col);
      fb.fillCircle(cx, cy - s - 5, 2, col);
      fb.drawRoundRect(cx - s, cy - s / 2, 2 * s, s + s / 2, 3, col);
      fb.fillCircle(cx - s / 2, cy, 3, col);
      fb.fillCircle(cx + s / 2, cy, 3, col);
      break;
    case 5:  // Toys: die with pips
      fb.drawRoundRect(cx - s, cy - s, 2 * s, 2 * s, 3, col);
      fb.fillCircle(cx - s / 2, cy - s / 2, 2, col);
      fb.fillCircle(cx + s / 2, cy - s / 2, 2, col);
      fb.fillCircle(cx, cy, 2, col);
      fb.fillCircle(cx - s / 2, cy + s / 2, 2, col);
      fb.fillCircle(cx + s / 2, cy + s / 2, 2, col);
      break;
    case 6:  // Settings: gear
      for (int a = 0; a < 360; a += 45) {
        float r = a * 3.14159f / 180;
        fb.fillRect(cx + cos(r) * s - 2, cy + sin(r) * s - 2, 4, 4, col);
      }
      fb.drawCircle(cx, cy, s - 2, col);
      fb.fillCircle(cx, cy, 3, col);
      break;
    case 7:   // Bluetooth rune
    case 8: { // ...with broadcast waves (spam)
      int t = s, h = s / 2;
      fb.drawLine(cx, cy - t, cx, cy + t, col);
      fb.drawLine(cx, cy - t, cx + h, cy - h, col);
      fb.drawLine(cx + h, cy - h, cx - h, cy + h, col);
      fb.drawLine(cx, cy + t, cx + h, cy + h, col);
      fb.drawLine(cx + h, cy + h, cx - h, cy - h, col);
      if (id == 8) {
        fb.drawArc(cx, cy, t + 7, t + 5, 305, 55, col);
        fb.drawArc(cx, cy, t + 12, t + 10, 305, 55, col);
      }
      break;
    }
    case 9:   // Clock: face + two hands
      fb.drawCircle(cx, cy, s, col);
      fb.drawLine(cx, cy, cx, cy - s * 2 / 3, col);        // minute hand up
      fb.drawLine(cx, cy, cx + s / 2, cy + s / 4, col);    // hour hand
      break;
    case 11:  // WiFi Watch: shield with alert bar
      fb.drawLine(cx - s, cy - s, cx + s, cy - s, col);
      fb.drawLine(cx - s, cy - s, cx - s, cy, col);
      fb.drawLine(cx + s, cy - s, cx + s, cy, col);
      fb.drawLine(cx - s, cy, cx, cy + s, col);
      fb.drawLine(cx + s, cy, cx, cy + s, col);
      fb.fillRect(cx - 1, cy - s / 2, 2, s - 2, col);   // exclamation
      fb.fillCircle(cx, cy + s / 2, 1, col);
      break;
    case 12:  // WiFi Spam: wifi arcs + extra broadcast ring
      for (int r = s; r > 0; r -= s / 3)
        fb.fillArc(cx, cy + s / 2, r, r - 2, 200, 340, col);
      fb.fillCircle(cx, cy + s / 2, 2, col);
      fb.drawArc(cx, cy + s / 2, s + 6, s + 4, 200, 340, col);
      break;
    case 13: { // BLE Hunter: bluetooth rune + magnifier lens
      int t = s, h = s / 2;
      fb.drawLine(cx, cy - t, cx, cy + t, col);
      fb.drawLine(cx, cy - t, cx + h, cy - h, col);
      fb.drawLine(cx + h, cy - h, cx - h, cy + h, col);
      fb.drawLine(cx, cy + t, cx + h, cy + h, col);
      fb.drawLine(cx + h, cy + h, cx - h, cy - h, col);
      fb.drawCircle(cx + s, cy + s, s / 2, col);
      fb.drawLine(cx + s + s / 3, cy + s + s / 3, cx + s + s, cy + s + s, col);
      break;
    }
    case 10:  // Flashlight: sun
      fb.fillCircle(cx, cy, s / 2, col);
      for (int a = 0; a < 360; a += 45) {
        float r = a * 3.14159f / 180;
        fb.drawLine(cx + cosf(r) * s * 0.72f, cy + sinf(r) * s * 0.72f,
                    cx + cosf(r) * s, cy + sinf(r) * s, col);
      }
      break;
  }
}

// One carousel card centered at cx. big = the focused card. The title sits
// BELOW the icon card (not inside it) so it can never touch/overflow a border.
static void draw_card(int i, int cx, bool big) {
  const int w = big ? 86 : 58, h = 68;
  int x = cx - w / 2, y = 24;
  uint16_t col = APPS[i].color;
  fb.drawRoundRect(x, y, w, h, 8, big ? col : TFT_DARKGREY);
  if (big) fb.drawRoundRect(x + 1, y + 1, w - 2, h - 2, 7, col);
  draw_icon(APPS[i].icon, cx, y + h / 2, big ? 20 : 14, big ? col : TFT_DARKGREY);
  if (big) {
    fb.setTextSize(2);
    fb.setTextDatum(top_center);
    fb.setTextColor(TFT_WHITE, TFT_BLACK);
    fb.drawString(APPS[i].name, cx, y + h + 8);   // centered under the card
    fb.setTextDatum(top_left);
  }
}

void launcher() {
  int sel = ui::last_card() % APP_COUNT;
  while (true) {
    fb.fillScreen(TFT_BLACK);
    fb.setTextSize(1);
    fb.setTextColor(TFT_DARKGREY, TFT_BLACK);
    fb.setCursor(4, 4);
    fb.print("StickS3");
    ui::draw_battery();

    int prev = (sel + APP_COUNT - 1) % APP_COUNT;
    int next = (sel + 1) % APP_COUNT;
    draw_card(prev, 18, false);          // left peek
    draw_card(next, ui::W - 18, false);  // right peek
    draw_card(sel, ui::W / 2, true);     // focused center

    // page dots
    for (int i = 0; i < APP_COUNT; i++) {
      int dx = ui::W / 2 - (APP_COUNT * 8) / 2 + i * 8 + 4;
      fb.fillCircle(dx, ui::H - 6, 2, i == sel ? APPS[sel].color : TFT_DARKGREY);
    }

    ui::Ev ev = ui::frame();
    if (ev.b) { sel = (sel + 1) % APP_COUNT; ui::set_last_card(sel); }   // next, wraps
    if (ev.a) APPS[sel].run();                                            // launch
  }
}
