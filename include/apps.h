#pragma once
#include <cstdint>

// App registry. Each app is a run() that owns its own loop and returns when
// ui::frame() reports ev.home (long-press). Adding an app = one entry here +
// one run() definition. No SD, no dynamic loading -- everything is compiled in.

struct App {
  const char *name;
  void (*run)();
  uint16_t color;   // carousel icon/accent color
  uint8_t icon;     // icon id, drawn by draw_icon() in launcher.cpp
};

// Defined per app file in src/apps/.
void app_wifi_scan();
void app_spectrum();
void app_audio_viz();
void app_ir_tools();
void app_ai_buddy();
void app_ble_scan();
void app_ble_spam();
void app_clock();
void app_flashlight();
void app_toys();
void app_settings();

extern const App APPS[];
extern const int APP_COUNT;
