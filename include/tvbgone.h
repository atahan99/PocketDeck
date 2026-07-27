#pragma once
#include <Arduino.h>
#include <pgmspace.h>

// TV-B-Gone power-code database interface. Data is the canonical Adafruit /
// Ken Shirriff port of Mitch Altman + Limor Fried's TV-B-Gone firmware
// (src/tvbgone_codes.cpp), NOT Bruce's AGPL fork -- keeps this repo clean.
//
// Codes are stored packed: times[] holds on/off pair lengths in 10us units,
// codes[] is a bitstream of `bitcompression`-bit indices into those pairs, and
// timer_val encodes the carrier. app_ir_tools unpacks them into raw microsecond
// buffers for IRremoteESP8266.

// The data file calls freq_to_timerval(HZ). ESP32 defines F_CPU as 240MHz which
// would break the original macro, so pin it to the AVR-8MHz form: this makes
// timer_val = 1000000/hz - 1, invertible below.
#ifdef freq_to_timerval
#undef freq_to_timerval
#endif
#define freq_to_timerval(x) (1000000UL / (x) - 1)

#ifndef NUM_ELEM
#define NUM_ELEM(x) (sizeof(x) / sizeof(*(x)))
#endif

struct IrCode {
  uint8_t timer_val;      // carrier: freq_to_timerval(hz)
  uint8_t numpairs;       // number of on/off pairs
  uint8_t bitcompression; // bits per index into times[]
  uint16_t const *times;  // pair lengths, 10us units
  uint8_t const *codes;   // packed bitstream
};

// Recover carrier in kHz from timer_val (inverse of the macro above).
static inline uint16_t tvb_freq_khz(uint8_t tv) { return 1000 / (tv + 1); }

extern const struct IrCode *NApowerCodes[];
extern const struct IrCode *EUpowerCodes[];
extern uint8_t num_NAcodes;
extern uint8_t num_EUcodes;
