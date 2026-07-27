#include <M5Unified.h>

#include "ui.h"

void launcher();  // src/launcher.cpp

void setup() {
  auto cfg = M5.config();
  cfg.internal_spk = false;    // AW8737 amp buzzes when left enabled at idle
  // Leave internal_mic enabled: M5.begin only fills the StickS3 mic pin config
  // when it's true, and Audio Viz's M5.Mic.begin() needs those pins. The mic
  // doesn't drive the amp, so it costs no buzz.
  M5.begin(cfg);
  M5.Speaker.end();            // ensure the amp path is actually down

  // Kill the idle buzz. Two sources, found by a layer-by-layer sweep:
  //  1. The 5V Grove boost converter whines unloaded -- the dominant hum. It's
  //     turned off in ui::init() (respecting the Settings toggle, default off).
  //  2. The backlight PWM defaults to 256 Hz (audible); re-init it ultrasonic.
  if (auto *bl = (lgfx::Light_PWM *)M5.Display.getPanel()->getLight()) {
    auto c = bl->config();
    c.freq = 44100;
    bl->config(c);
    bl->init(M5.Display.getBrightness());
  }

  Serial.begin(115200);
  ui::init();                  // sets landscape orientation + framebuffer
}

void loop() {
  launcher();   // never returns; owns the app dispatch loop
}
