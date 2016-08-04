


#ifndef wm8816_h
#define wm8816_h

#include <stdio.h>
#include <Arduino.h>

/* Register values */
#define REG_PEAK_DETECTOR_STATUS 0b11011111
#define REG_PEAK_DETECTOR_REF    0b11100111
#define REG_LEFT_CHANNEL_GAIN    0b11101111
#define REG_RIGHT_CHANNEL_GAIN   0b11110111
#define REG_BOTH_CHANNEL_GAIN    0b11001011

/* Value defined for Peak Detector Status Register. */
#define PEAK_STATUS_NO_OVERLOAD    0b00000000
#define PEAK_STATUS_RIGHT_OVERLOAD 0b00000001
#define PEAK_STATUS_LEFT_OVERLOAD  0b00000010
#define PEAK_STATUS_BOTH_OVERLOAD  0b00000011

/* Value defined to read/write channels gains registers. */
#define MUTE    0b00000000
#define ZERO_DB 0b11100000
#define MAX     0b11111111

class wm8816 {
public:
  wm8816(uint8_t csbPin, uint8_t mutebPin, uint8_t dataPin, uint8_t cclkPin);
  void init();
  void write(byte reg, byte data);
  void read(byte reg, byte *data);
  void mute();
  void unmute();
};

typedef struct {
  uint8_t csbPin;
  uint8_t mutebPin;
  uint8_t dataPin;
  uint8_t cclkPin;
} wm_pin_t;

extern wm_pin_t pin;
#endif