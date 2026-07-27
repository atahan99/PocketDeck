#pragma once
#include <M5Unified.h>

// Shared UI framework for every app. One PSRAM framebuffer everything draws
// into; one frame() call per loop that pushes it, services the USB-serial
// bridge (screenshots + injected keys), and returns this frame's input.
//
// Contract every app follows: draw into `fb`, then call frame(). Return from
// the app's loop when ev.home is set (long-press) to fall back to the launcher.
// Because screenshots dump `fb` inside frame(), a host can snapshot any screen.

namespace ui {

extern M5Canvas fb;              // RGB565 backbuffer in PSRAM
extern int W, H;                 // current framebuffer size; flips with orient()

struct Ev {                      // edge events since last frame
  bool a = false;                // BtnA tap: select / action (immediate)
  bool b = false;                // BtnB tap: next / move (immediate)
  bool pwr = false;              // power btn tap
  bool home = false;             // long-press: back to launcher
  bool b_single = false;         // BtnB single-click, decided (delayed; excludes doubles)
  bool b_double = false;         // BtnB double-click
};

void init();                     // call once after M5.begin (starts landscape)
void orient(bool landscape);     // switch orientation; recreates fb, updates W/H
Ev frame(uint32_t delay_ms = 16);// push fb, poll bridge, idle-manage, return input

// Small shared helpers so apps don't each reinvent them.
void header(const char *title, uint16_t color = TFT_GREEN);
void toast(const char *msg);     // full-screen centered line, pushes immediately
void draw_battery();             // small battery glyph + % at the top-right

// One frame of a standard vertical-list menu (header + accent-highlighted rows).
// Call in a loop. Returns the chosen index on BtnA, MENU_BACK on long-press,
// MENU_NONE otherwise; advances *sel on BtnB.
constexpr int MENU_NONE = -1, MENU_BACK = -2;
int menu(const char *title, uint16_t color, const char *const *items, int n, int &sel);

// Power / persistence.
extern uint8_t user_brightness;  // 15..255, persisted in NVS
void set_brightness(uint8_t b);  // apply + save
void stay_awake();               // reset the idle timer; continuously-drawing apps call this each frame
int battery_level();             // 0..100 (-1 if unknown)
bool charging();
int last_card();                 // persisted launcher selection
void set_last_card(int i);
bool grove_5v();                 // Grove PORT.A 5V boost (off by default; whines unloaded)
void set_grove_5v(bool on);      // apply + persist

}  // namespace ui
