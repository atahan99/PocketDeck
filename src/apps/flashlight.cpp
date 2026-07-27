#include "apps.h"
#include "ui.h"

// Full-white screen at max brightness. Long-press to exit (restores brightness).
void app_flashlight() {
  uint8_t save = ui::user_brightness;
  M5.Display.setBrightness(255);
  while (true) {
    ui::fb.fillScreen(TFT_WHITE);
    ui::stay_awake();
    if (ui::frame().home) break;
  }
  M5.Display.setBrightness(save);
}
