#pragma once

/// Datatype for Pins: Use int16_t to allow -1 for "use default"
using sai_pin_t = int16_t;  

/// Pin identifiers for SAI signals
enum class PinId { SCK, FS, SD, MCLK, NumPins };

struct PinConfig {
  int8_t port = -1;  ///< Port letter as ASCII ('A'=65, ...)
  int8_t pin = -1;   ///< Pin number
  int8_t af = -1;    ///< Alternate function
  PinConfig(int8_t pt = -1, int8_t pn = -1, int8_t a = -1)
      : port(pt), pin(pn), af(a) {}
};
