#include <Preferences.h>
#include <WiFi.h>
#include <driver/gpio.h>
#include <esp_sleep.h>

#include "ui.h"

// USB-serial bridge: same idea as m5dial's dial.py but over the cable, since
// the S3 has PSRAM for a full framebuffer. Host commands, newline-terminated:
//   shot        -> device replies:  "\n<<SHOT W H>>\n" then W*H*2 raw bytes
//   key a|b|pwr|home  -> injects that press into the next frame
// Everything else the device prints is plain log text (read with a serial monitor).

namespace ui {

M5Canvas fb(&M5.Display);
int W = 240, H = 135;            // landscape default

static Ev g_inject;              // keys injected over serial, merged next frame
static bool g_shot_req = false;
static char g_line[32];
static uint8_t g_len = 0;

// Power management: dim then sleep on inactivity. Continuously-drawing apps
// (spectrum, audio, spam) call stay_awake() so they don't dim under the user.
uint8_t user_brightness = 180;
static Preferences prefs;
static uint32_t idle_since = 0;
static bool dimmed = false;
constexpr uint32_t DIM_MS = 20000, SLEEP_MS = 120000;
constexpr uint8_t DIM_LEVEL = 16;

void stay_awake() {
  idle_since = millis();
  if (dimmed) { M5.Display.setBrightness(user_brightness); dimmed = false; }
}

void set_brightness(uint8_t b) {
  user_brightness = b;
  M5.Display.setBrightness(b);
  prefs.putUChar("bri", b);
}

int battery_level() { return M5.Power.getBatteryLevel(); }
bool charging() { return M5.Power.isCharging(); }
int last_card() { return prefs.getInt("card", 0); }
void set_last_card(int i) { prefs.putInt("card", i); }
bool grove_5v() { return prefs.getBool("grove5v", false); }
void set_grove_5v(bool on) { prefs.putBool("grove5v", on); M5.Power.setExtOutput(on); }

bool wifi_on() { return prefs.getBool("wifi", false); }
void set_wifi_on(bool on) {
  prefs.putBool("wifi", on);
  if (on) { WiFi.mode(WIFI_STA); WiFi.begin(); }           // reconnect saved network
  else { WiFi.disconnect(true, true); WiFi.mode(WIFI_OFF); }  // radio fully off
}

// Only deep-sleep when actually running on the battery. On USB there's nothing
// to save, and light sleep suspends the USB CDC link (and blocks re-flashing).
static bool on_battery() {
  return M5.Power.M5pm1.getPowerSource() == m5::M5PM1_Class::battery;
}

static void deep_idle_sleep() {
  M5.Display.sleep();                        // panel + backlight off (biggest draw)
  gpio_wakeup_enable(GPIO_NUM_11, GPIO_INTR_LOW_LEVEL);   // BtnA
  gpio_wakeup_enable(GPIO_NUM_12, GPIO_INTR_LOW_LEVEL);   // BtnB
  esp_sleep_enable_gpio_wakeup();
  esp_light_sleep_start();                   // resumes here on a button press
  M5.Display.wakeup();
  M5.Display.setBrightness(user_brightness);
  dimmed = false;
  idle_since = millis();
  delay(200);                                // debounce the wake press
}

void init() {
  fb.setPsram(true);
  fb.setColorDepth(16);
  orient(true);
  fb.setTextColor(TFT_WHITE, TFT_BLACK);
  prefs.begin("ui", false);
  user_brightness = prefs.getUChar("bri", 180);
  M5.Display.setBrightness(user_brightness);
  M5.Power.setExtOutput(prefs.getBool("grove5v", false));   // Grove 5V off by default (buzz)
  if (prefs.getBool("wifi", false)) { WiFi.mode(WIFI_STA); WiFi.begin(); }
  else { WiFi.mode(WIFI_OFF); }                             // radio off by default, not broadcasting
  idle_since = millis();
}

void orient(bool landscape) {
  M5.Display.setRotation(landscape ? 1 : 0);
  W = landscape ? 240 : 135;
  H = landscape ? 135 : 240;
  if (fb.getBuffer()) fb.deleteSprite();
  fb.createSprite(W, H);
}

static void handle_cmd(const char *s) {
  if (!strcmp(s, "shot")) {
    g_shot_req = true;
  } else if (!strncmp(s, "key ", 4)) {
    const char *k = s + 4;
    if (!strcmp(k, "a")) g_inject.a = true;
    else if (!strcmp(k, "b")) { g_inject.b = true; g_inject.b_single = true; }  // a real single click sets both
    else if (!strcmp(k, "bb")) g_inject.b_double = true;                         // double-click B
    else if (!strcmp(k, "pwr")) g_inject.pwr = true;
    else if (!strcmp(k, "home")) g_inject.home = true;
  }
}

static void poll_serial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (g_len) { g_line[g_len] = 0; handle_cmd(g_line); g_len = 0; }
    } else if (g_len < sizeof(g_line) - 1) {
      g_line[g_len++] = c;
    }
  }
}

static void send_shot() {
  Serial.printf("\n<<SHOT %d %d>>\n", W, H);
  Serial.flush();
  const uint8_t *buf = (const uint8_t *)fb.getBuffer();
  Serial.write(buf, (size_t)W * H * 2);
  Serial.flush();
  g_shot_req = false;
}

Ev frame(uint32_t delay_ms) {
  fb.pushSprite(0, 0);
  poll_serial();
  if (g_shot_req) send_shot();

  M5.update();
  Ev ev;
  ev.a = M5.BtnA.wasClicked() || g_inject.a;
  ev.b = M5.BtnB.wasClicked() || g_inject.b;
  ev.pwr = M5.BtnPWR.wasClicked() || g_inject.pwr;
  ev.home = M5.BtnA.wasHold() || M5.BtnB.wasHold() || g_inject.home;
  ev.b_single = M5.BtnB.wasSingleClicked() || g_inject.b_single;   // decided single (no double)
  ev.b_double = M5.BtnB.wasDoubleClicked() || g_inject.b_double;
  g_inject = Ev{};

  // Hold the power button to sleep on demand (deliberate, so unlike the idle
  // auto-sleep it fires on USB too).
  if (M5.BtnPWR.wasHold()) deep_idle_sleep();

  // Idle management: input (real or injected) keeps it awake; else dim, then sleep.
  if (ev.a || ev.b || ev.pwr || ev.home) stay_awake();
  uint32_t idle = millis() - idle_since;
  if (!dimmed && idle > DIM_MS) { M5.Display.setBrightness(DIM_LEVEL); dimmed = true; }
  if (idle > SLEEP_MS && on_battery()) deep_idle_sleep();

  delay(delay_ms);
  return ev;
}

void draw_battery() {
  int lvl = M5.Power.getBatteryLevel();
  if (lvl < 0) return;
  if (lvl <= 10 && !charging() && (millis() / 500) % 2) return;   // blink when critically low
  const int w = 22, h = 10, x = W - w - 6, y = 2;
  uint16_t c = charging() ? TFT_GREEN : lvl <= 20 ? TFT_RED : TFT_WHITE;
  fb.drawRect(x, y, w, h, c);
  fb.fillRect(x + w, y + 3, 2, h - 6, c);                 // terminal nub
  fb.fillRect(x + 1, y + 1, (w - 2) * (lvl > 100 ? 100 : lvl) / 100, h - 2, c);
  fb.setTextSize(1);
  fb.setTextColor(TFT_DARKGREY, TFT_BLACK);
  fb.setTextDatum(top_right);
  char s[8];
  snprintf(s, sizeof(s), "%s%d%%", charging() ? "+" : "", lvl);   // e.g. "+87%"
  fb.drawString(s, x - 3, 3);
  fb.setTextDatum(top_left);
}

void header(const char *title, uint16_t color) {
  fb.fillScreen(TFT_BLACK);
  fb.setTextSize(1);
  fb.setTextColor(color, TFT_BLACK);
  fb.setCursor(4, 4);
  fb.print(title);
  fb.drawFastHLine(0, 15, W, color);
  draw_battery();
}

int menu(const char *title, uint16_t color, const char *const *items, int n, int &sel) {
  header(title, color);
  fb.setTextSize(2);
  int rowh = (H - 22) / n;
  if (rowh > 30) rowh = 30;
  for (int i = 0; i < n; i++) {
    int y = 20 + i * rowh;
    bool on = i == sel;
    if (on) fb.fillRoundRect(4, y, W - 8, rowh - 3, 4, color);
    fb.setTextColor(on ? TFT_BLACK : color, TFT_BLACK);
    fb.setCursor(10, y + (rowh - 16) / 2);
    fb.print(items[i]);
  }
  fb.setTextSize(1);
  Ev ev = frame();
  if (ev.home) return MENU_BACK;
  if (ev.b) sel = (sel + 1) % n;
  if (ev.a) return sel;
  return MENU_NONE;
}

void toast(const char *msg) {
  fb.fillScreen(TFT_BLACK);
  fb.setTextColor(TFT_WHITE, TFT_BLACK);
  fb.setTextSize(1);
  fb.setTextDatum(middle_center);
  fb.drawString(msg, W / 2, H / 2);
  fb.setTextDatum(top_left);
  fb.pushSprite(0, 0);
}

}  // namespace ui
