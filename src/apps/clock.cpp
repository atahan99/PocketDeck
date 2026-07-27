#include "apps.h"
#include "ui.h"

using ui::fb;

// Time tools -- fully offline (no clock/WiFi): stopwatch, countdown timer,
// pomodoro, and an interval (Tabata) timer. Each is watched more than pressed,
// so they call ui::stay_awake().

namespace {

void big_time(const char *s, uint16_t col, int y = 46) {
  fb.setTextDatum(middle_center);
  fb.setTextColor(col, TFT_BLACK);
  fb.setTextSize(4);
  fb.drawString(s, ui::W / 2, y);
  fb.setTextSize(1);
  fb.setTextDatum(top_left);
}

// ---- Stopwatch ---------------------------------------------------------
void page_stopwatch() {
  uint32_t start = 0, acc = 0;
  bool run = false;
  char buf[16];
  while (true) {
    ui::stay_awake();
    ui::header("Stopwatch", TFT_GREEN);
    uint32_t ms = acc + (run ? millis() - start : 0);
    snprintf(buf, sizeof(buf), "%02lu:%02lu.%lu", (unsigned long)(ms / 60000),
             (unsigned long)((ms / 1000) % 60), (unsigned long)((ms / 100) % 10));
    big_time(buf, run ? TFT_GREEN : TFT_WHITE, 56);
    fb.setTextColor(TFT_DARKGREY, TFT_BLACK);
    fb.setCursor(6, ui::H - 10);
    fb.print("A=start/stop  B=reset");

    ui::Ev ev = ui::frame(33);
    if (ev.home) break;
    if (ev.a) { if (run) { acc = ms; run = false; } else { start = millis(); run = true; } }
    if (ev.b && !run) acc = 0;
  }
}

// ---- Timer -------------------------------------------------------------
void page_timer() {
  int mins = 5;
  uint32_t end = 0;
  bool run = false, done = false;
  char buf[16];
  while (true) {
    ui::stay_awake();
    uint32_t rem = run ? (end > millis() ? end - millis() : 0) : (uint32_t)mins * 60000;
    if (run && rem == 0 && !done) { done = true; run = false; }
    ui::header("Timer", done ? TFT_RED : TFT_ORANGE);
    if (done && (millis() / 400) % 2) fb.fillScreen(TFT_RED);   // flash
    snprintf(buf, sizeof(buf), "%02lu:%02lu", (unsigned long)(rem / 60000),
             (unsigned long)((rem / 1000) % 60));
    big_time(buf, done ? TFT_WHITE : TFT_ORANGE, 56);
    fb.setTextColor(TFT_DARKGREY, TFT_BLACK);
    fb.setCursor(6, ui::H - 10);
    fb.print(run ? "A=pause  hold=exit" : "A=start  B=+1  BB=-1");

    ui::Ev ev = ui::frame(100);
    if (ev.home) break;
    if (ev.a) {
      if (done) { done = false; run = false; }
      else if (run) { mins = rem / 60000; run = false; }        // pause: keep remaining
      else if (mins > 0) { end = millis() + (uint32_t)mins * 60000; run = true; }
    }
    if (!run && !done) {
      if (ev.b_single) mins = mins >= 99 ? 1 : mins + 1;         // single-click: +1
      if (ev.b_double) mins = mins <= 1 ? 99 : mins - 1;         // double-click: -1
    }
  }
}

// ---- Interval / Tabata -------------------------------------------------
void page_interval() {
  const int WORK = 20, REST = 10, ROUNDS = 8;   // classic Tabata
  int round = 1;
  bool work = true, run = false;
  uint32_t end = 0, remStore = WORK * 1000;
  char buf[16];
  while (true) {
    ui::stay_awake();
    uint32_t rem = run ? (end > millis() ? end - millis() : 0) : remStore;
    if (run && rem == 0) {
      if (work) { work = false; remStore = REST * 1000; }
      else { work = true; remStore = WORK * 1000; if (++round > ROUNDS) { run = false; round = 1; } }
      end = millis() + remStore;
      rem = remStore;
    }
    ui::header("Interval", work ? TFT_RED : TFT_GREEN);
    fb.setTextColor(work ? TFT_RED : TFT_GREEN, TFT_BLACK);
    fb.setTextDatum(top_center);
    fb.drawString(work ? "WORK" : "REST", ui::W / 2, 20);
    fb.setTextDatum(top_left);
    snprintf(buf, sizeof(buf), "%02lu", (unsigned long)((rem + 999) / 1000));
    big_time(buf, work ? TFT_RED : TFT_GREEN, 62);
    fb.setTextColor(TFT_DARKGREY, TFT_BLACK);
    fb.setCursor(6, ui::H - 10);
    fb.printf("round %d/%d  A=start B=reset", round, ROUNDS);

    ui::Ev ev = ui::frame(100);
    if (ev.home) break;
    if (ev.a) { if (run) { remStore = rem; run = false; } else { end = millis() + rem; run = true; } }
    if (ev.b) { run = false; work = true; round = 1; remStore = WORK * 1000; }
  }
}

// ---- Pomodoro ----------------------------------------------------------
void page_pomodoro() {
  const int WORK = 25, BREAK = 5;
  bool onBreak = false, run = false;
  uint32_t end = 0;
  uint32_t remStore = WORK * 60000;
  char buf[16];
  while (true) {
    ui::stay_awake();
    uint32_t rem = run ? (end > millis() ? end - millis() : 0) : remStore;
    if (run && rem == 0) { onBreak = !onBreak; remStore = (onBreak ? BREAK : WORK) * 60000; end = millis() + remStore; }
    ui::header("Pomodoro", onBreak ? TFT_GREEN : TFT_RED);
    fb.setTextColor(onBreak ? TFT_GREEN : TFT_RED, TFT_BLACK);
    fb.setTextDatum(top_center);
    fb.drawString(onBreak ? "BREAK" : "FOCUS", ui::W / 2, 20);
    fb.setTextDatum(top_left);
    snprintf(buf, sizeof(buf), "%02lu:%02lu", (unsigned long)(rem / 60000),
             (unsigned long)((rem / 1000) % 60));
    big_time(buf, onBreak ? TFT_GREEN : TFT_RED, 62);
    fb.setTextColor(TFT_DARKGREY, TFT_BLACK);
    fb.setCursor(6, ui::H - 10);
    fb.print("A=start/pause  B=reset");

    ui::Ev ev = ui::frame(100);
    if (ev.home) break;
    if (ev.a) { if (run) { remStore = rem; run = false; } else { end = millis() + rem; run = true; } }
    if (ev.b) { run = false; onBreak = false; remStore = WORK * 60000; }
  }
}

struct Page { const char *name; void (*run)(); };
const Page PAGES[] = {
    {"Stopwatch", page_stopwatch}, {"Timer", page_timer},
    {"Pomodoro", page_pomodoro}, {"Interval", page_interval},
};
const int NP = sizeof(PAGES) / sizeof(PAGES[0]);
}  // namespace

void app_clock() {
  const char *items[NP];
  for (int i = 0; i < NP; i++) items[i] = PAGES[i].name;
  int sel = 0;
  while (true) {
    int r = ui::menu("Time Tools", TFT_CYAN, items, NP, sel);
    if (r == ui::MENU_BACK) return;
    if (r >= 0) PAGES[r].run();
  }
}
