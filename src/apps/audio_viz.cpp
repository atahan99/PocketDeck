#include <arduinoFFT.h>

#include "apps.h"
#include "ui.h"

using ui::fb;

// Ambient audio spectrum off the built-in ES8311 mic. Record a block, FFT it,
// draw log-ish frequency bars that react to surrounding music. BtnA cycles
// sensitivity, long-press exits.
//
// ponytail: GAIN is a hand-tuned calibration knob -- mic sensitivity and room
// level vary, so the scale factor is empirical, not derived. Bump it with BtnA.

namespace {
constexpr int N = 256;              // FFT size (power of 2)
constexpr int RATE = 16000;
constexpr int BARS = 24;
constexpr float GAINS[] = {0.5f, 1.0f, 2.0f, 4.0f};

float vReal[N], vImag[N];
int16_t pcm[N];
ArduinoFFT<float> FFT(vReal, vImag, N, RATE);
}  // namespace

void app_audio_viz() {
  M5.Mic.begin();                   // amp stays off; this app only listens

  float bar[BARS] = {0};
  int gi = 1;

  while (true) {
    if (M5.Mic.record(pcm, N, RATE)) {
      while (M5.Mic.isRecording()) M5.update();
      for (int i = 0; i < N; i++) { vReal[i] = pcm[i]; vImag[i] = 0; }
      FFT.windowing(FFTWindow::Hamming, FFTDirection::Forward);
      FFT.compute(FFTDirection::Forward);
      FFT.complexToMagnitude();

      // Group the usable bins (skip DC) into BARS buckets.
      const int usable = N / 2;
      for (int b = 0; b < BARS; b++) {
        int lo = 1 + b * usable / BARS, hi = 1 + (b + 1) * usable / BARS;
        float m = 0;
        for (int i = lo; i < hi; i++) m += vReal[i];
        m = m / (hi - lo) * GAINS[gi] / 6000.0f;
        bar[b] = bar[b] * 0.6f + m * 0.4f;      // smooth + decay
      }
    }

    ui::stay_awake();   // watched, not touched
    ui::header("Audio Viz", TFT_MAGENTA);
    const int base = 132, top = 18, bw = ui::W / BARS;
    for (int b = 0; b < BARS; b++) {
      int h = (int)(bar[b] * (base - top));
      if (h > base - top) h = base - top;
      if (h < 1) h = 1;
      uint16_t col = h > 80 ? TFT_RED : h > 45 ? TFT_YELLOW : TFT_GREEN;
      fb.fillRect(b * bw + 1, base - h, bw - 1, h, col);
    }
    fb.setTextColor(TFT_DARKGREY, TFT_BLACK);
    fb.setCursor(180, 4);
    fb.printf("gain %gx", GAINS[gi]);

    ui::Ev ev = ui::frame(0);
    if (ev.home) break;
    if (ev.a) gi = (gi + 1) % (sizeof(GAINS) / sizeof(GAINS[0]));
  }

  M5.Mic.end();                     // leave the amp down; nothing plays audio
}
