#pragma once
#include <stdint.h>

/// Datatype for Pins: Use int16_t to allow -1 for "use default"
using sai_pin_t = int16_t;  

/// Pin identifiers for SAI signals. SD is the data pin used when TX/RX share
/// a single SAI block (or for TX/output on boards with separate blocks).
/// SD_RX is only needed on boards where TX and RX are separate SAI blocks
/// wired to different physical data pins (e.g. the STM32F723E-Discovery's
/// SAI2 Block A/Block B) - boards with a single data pin leave it unset.
enum class PinId { SCK, FS, SD, SD_RX, MCLK, NumPins };

struct PinConfig {
  int8_t port = -1;  ///< Port letter as ASCII ('A'=65, ...)
  int8_t pin = -1;   ///< Pin number
  int8_t af = -1;    ///< Alternate function
  PinConfig(int8_t pt = -1, int8_t pn = -1, int8_t a = -1)
      : port(pt), pin(pn), af(a) {}
};
