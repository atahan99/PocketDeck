#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>
#include <Preferences.h>

#include "apps.h"
#include "flipper_ac_off.h"
#include "tvbgone.h"
#include "ui.h"

using ui::fb;

// IR on the StickS3's built-in emitter (G46) + receiver (G42):
//   TV-B-Gone NA/EU -- sweep the TV-B-Gone power-code DB, shut off nearby TVs
//   AC-B-Gone       -- blast the Flipper universal AC "Off" raw codes
//   Univ. Remote    -- learn a real remote's button on G42, replay it on G46
//   IR Self-Test    -- loopback to confirm the emitter actually transmits
// BtnB scrolls, BtnA runs, long-press aborts a sweep or exits the menu.

namespace {
constexpr uint16_t IR_LED = 46;
constexpr uint16_t IR_RX = 42;
IRsend irsend(IR_LED);

// ---- Universal remote: 3 slots, persisted to NVS so learned remotes survive reboot.
constexpr int SLOTS = 3, RAWMAX = 300;
uint16_t slot_raw[SLOTS][RAWMAX];
uint16_t slot_len[SLOTS] = {0};

void ir_save(int slot) {
  Preferences p;
  p.begin("ir", false);
  char k[4];
  snprintf(k, sizeof(k), "l%d", slot);
  p.putUShort(k, slot_len[slot]);
  snprintf(k, sizeof(k), "s%d", slot);
  p.putBytes(k, slot_raw[slot], slot_len[slot] * sizeof(uint16_t));
  p.end();
}

void ir_load() {
  Preferences p;
  p.begin("ir", true);
  for (int i = 0; i < SLOTS; i++) {
    char k[4];
    snprintf(k, sizeof(k), "l%d", i);
    slot_len[i] = p.getUShort(k, 0);
    if (slot_len[i] > RAWMAX) slot_len[i] = 0;
    if (slot_len[i]) { snprintf(k, sizeof(k), "s%d", i); p.getBytes(k, slot_raw[i], slot_len[i] * sizeof(uint16_t)); }
  }
  p.end();
}

void ir_learn(int slot) {
  ui::toast("Aim remote, press a\nbutton... (8s)");
  IRrecv irrecv(IR_RX, 1024, 15, true);
  irrecv.enableIRIn();
  decode_results r;
  uint32_t end = millis() + 8000;
  bool got = false;
  while (millis() < end) {
    if (irrecv.decode(&r)) {
      uint16_t len = getCorrectedRawLength(&r);
      if (len > RAWMAX) len = RAWMAX;
      uint16_t *raw = resultToRawArray(&r);
      memcpy(slot_raw[slot], raw, len * sizeof(uint16_t));
      delete[] raw;
      slot_len[slot] = len;
      ir_save(slot);        // persist so it survives a reboot
      got = true;
      break;
    }
    M5.update();
    if (M5.BtnA.wasHold() || M5.BtnB.wasHold()) break;
    delay(5);
  }
  irrecv.disableIRIn();
  ui::toast(got ? "Captured!" : "Nothing heard");
  delay(900);
}

void universal_remote() {
  ir_load();               // restore learned slots from NVS
  int sel = 0;
  while (true) {
    ui::header("Univ. Remote", TFT_ORANGE);
    fb.setTextSize(2);
    for (int i = 0; i < SLOTS; i++) {
      int y = 24 + i * 30;
      bool on = i == sel;
      if (on) fb.fillRoundRect(4, y - 2, ui::W - 8, 26, 4, TFT_MAROON);
      fb.setTextColor(on ? TFT_WHITE : TFT_ORANGE, on ? TFT_MAROON : TFT_BLACK);
      fb.setCursor(10, y + 3);
      fb.printf("Slot %d: %s", i + 1, slot_len[i] ? "SET" : "empty");
    }
    fb.setTextSize(1);
    fb.setTextColor(TFT_DARKGREY, TFT_BLACK);
    fb.setCursor(6, ui::H - 10);
    fb.print(slot_len[sel] ? "A=replay B=next hold=exit" : "A=learn B=next hold=exit");

    ui::Ev ev = ui::frame();
    if (ev.home) return;
    if (ev.b) sel = (sel + 1) % SLOTS;
    if (ev.a) {
      if (slot_len[sel]) { irsend.sendRaw(slot_raw[sel], slot_len[sel], 38); ui::toast("Sent"); delay(400); }
      else ir_learn(sel);
    }
  }
}
uint16_t raw[600];   // numpairs*2 mark/space microseconds; DB max ~70 pairs

// Unpack one packed TV-B-Gone code and blast it via IRremoteESP8266.
void blast_tvb(const IrCode *code) {
  uint8_t numpairs = pgm_read_byte(&code->numpairs);
  uint8_t bitc = pgm_read_byte(&code->bitcompression);
  uint8_t tv = pgm_read_byte(&code->timer_val);
  const uint16_t *times = (const uint16_t *)pgm_read_ptr(&code->times);
  const uint8_t *codes = (const uint8_t *)pgm_read_ptr(&code->codes);

  uint16_t byteidx = 0, used = 0;
  uint8_t cur = pgm_read_byte(&codes[0]);
  auto read_bits = [&](uint8_t n) -> uint8_t {
    uint8_t r = 0;
    while (n--) {
      if (used == 8) { cur = pgm_read_byte(&codes[++byteidx]); used = 0; }
      r = (r << 1) | ((cur >> (7 - used)) & 1);
      used++;
    }
    return r;
  };

  int n = 0;
  for (uint8_t p = 0; p < numpairs; p++) {
    uint8_t idx = read_bits(bitc);
    raw[n++] = (uint16_t)pgm_read_word(&times[idx * 2]) * 10;      // mark us
    raw[n++] = (uint16_t)pgm_read_word(&times[idx * 2 + 1]) * 10;  // space us
  }
  irsend.sendRaw(raw, n, tvb_freq_khz(tv));
}

// AC-B-Gone: blast every "Off" code from the vendored Flipper universal AC DB
// (include/flipper_ac_off.h). Brand-agnostic raw frames -- covers the split ACs
// a named-protocol library misses. Aim the top edge at the unit.
void run_ac_bgone() {
  for (int i = 0; i < AC_OFF_COUNT; i++) {
    fb.fillScreen(TFT_BLACK);
    ui::header("AC-B-Gone", TFT_RED);
    fb.setTextColor(TFT_WHITE, TFT_BLACK);
    fb.setCursor(6, 55);
    fb.printf("%d / %d", i + 1, AC_OFF_COUNT);
    fb.drawRect(6, 78, ui::W - 12, 12, TFT_DARKGREY);
    fb.fillRect(6, 78, (ui::W - 12) * (i + 1) / AC_OFF_COUNT, 12, TFT_RED);
    fb.setCursor(6, 100);
    fb.print("Flipper AC codes");
    if (ui::frame(0).home) return;

    const uint16_t *d = (const uint16_t *)pgm_read_ptr(&AC_OFF_CODES[i].data);
    uint16_t len = pgm_read_word(&AC_OFF_CODES[i].len);
    irsend.sendRaw(d, len, 38);   // ESP32 flash is memory-mapped; direct read is fine
    delay(50);
  }
}

// Emitter test: continuously fire the IR LED (TX only -- exactly what TV/AC-B-Gone
// do) so a PHONE CAMERA can confirm it works. IR shows up as a flickering
// purple/white dot on screen. A loopback via the on-board receiver is unreliable
// (emitter + receiver face the same way, and TX+RX together can fight over RMT),
// so we test emission directly instead.
void ir_selftest() {
  uint32_t n = 0;
  while (true) {
    ui::header("IR Emit Test", TFT_ORANGE);
    fb.setTextColor(TFT_WHITE, TFT_BLACK);
    fb.setCursor(4, 22);
    fb.print("Point a PHONE CAMERA");
    fb.setCursor(4, 34);
    fb.print("(front cam) at the top.");
    fb.setTextColor(TFT_GREEN, TFT_BLACK);
    fb.setCursor(4, 54);
    fb.print("The IR LED should");
    fb.setCursor(4, 66);
    fb.print("flicker on the screen.");
    fb.setTextColor(TFT_DARKGREY, TFT_BLACK);
    fb.setCursor(4, ui::H - 10);
    fb.printf("blasting #%lu   hold=back", (unsigned long)n);

    irsend.sendNEC(0x20DF10EFULL);   // fire the emitter
    n++;
    if (ui::frame(70).home) return;  // ~12 blasts/sec -> easy camera flicker
  }
}

void run_tvb(const struct IrCode **region, uint8_t total) {
  for (int i = 0; i < total; i++) {
    fb.fillScreen(TFT_BLACK);
    ui::header("TV-B-Gone", TFT_ORANGE);
    fb.setTextColor(TFT_WHITE, TFT_BLACK);
    fb.setCursor(6, 55);
    fb.printf("%d / %d", i + 1, total);
    // simple progress bar
    fb.drawRect(6, 80, ui::W - 12, 12, TFT_DARKGREY);
    fb.fillRect(6, 80, (ui::W - 12) * (i + 1) / total, 12, TFT_ORANGE);
    if (ui::frame(0).home) return;

    const IrCode *code = (const IrCode *)pgm_read_ptr(&region[i]);
    blast_tvb(code);
    delay(205);                           // original TV-B-Gone inter-code gap
  }
}
}  // namespace

void app_ir_tools() {
  // CRITICAL: on the StickS3 the IR TX/RX are powered from the EXT_5V rail, which
  // is off by default (it's the idle-buzz source). Without this the emitter and
  // receiver are dead -- nothing transmits. Power it for the whole IR session and
  // restore the user's Grove-5V setting on exit. Buzz while in here is expected.
  M5.Power.setExtOutput(true);
  irsend.begin();

  const char *items[] = {"TV-B-Gone NA", "TV-B-Gone EU", "AC-B-Gone", "Univ. Remote", "IR Emit Test"};
  int sel = 0;
  while (true) {
    int r = ui::menu("IR Tools", TFT_ORANGE, items, 5, sel);
    if (r == ui::MENU_BACK) break;
    if (r == 0) run_tvb(NApowerCodes, num_NAcodes);
    else if (r == 1) run_tvb(EUpowerCodes, num_EUcodes);
    else if (r == 2) run_ac_bgone();
    else if (r == 3) universal_remote();
    else if (r == 4) ir_selftest();
  }
  M5.Power.setExtOutput(ui::grove_5v());   // restore (default off = no buzz)
}
