#pragma once
#include <Arduino.h>
#include <stdint.h>

/// Datatype for Pins: use the Arduino core PinName encoding.
using sai_pin_t = PinName;

/// Pin identifiers for SAI signals. SD is the data pin used when TX/RX share
/// a single SAI block (or for TX/output on boards with separate blocks).
/// SD_RX is only needed on boards where TX and RX are separate SAI blocks
/// wired to different physical data pins (e.g. the STM32F723E-Discovery's
/// SAI2 Block A/Block B) - boards with a single data pin leave it unset.
enum class PinId { SCK, FS, SD, SD_RX, MCLK, NumPins };

struct PinConfig {
  PinName pin = NC;  ///< Arduino-style pin name, e.g. PA5
  int8_t af = -1;    ///< Alternate function

  PinConfig(PinName p = NC, int8_t a = -1) : pin(p), af(a) {}
};
