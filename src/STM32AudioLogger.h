#pragma once
#include <Arduino.h>

class STM32AudioLogger {
 public:
  enum Level {
    NONE = 0,
    ERROR = 1,
    WARN = 2,
    INFO = 3,
    DEBUG = 4
  };

  static STM32AudioLogger& instance() {
    static STM32AudioLogger _instance;
    return _instance;
  }

  void begin(Level level, Print& out = Serial) {
    setLevel(level);
    setOutput(out);
  }

  void setOutput(Print& out) { output = &out; }
  void setLevel(Level level) { logLevel = level; }
  Level getLevel() const { return logLevel; }

  void error(const char* msg) { log(ERROR, "[ERROR] ", msg); }
  void warn(const char* msg) { log(WARN, "[WARN]  ", msg); }
  void info(const char* msg) { log(INFO, "[INFO]  ", msg); }
  void debug(const char* msg) { log(DEBUG, "[DEBUG] ", msg); }

  void errorf(const char* fmt, ...) {
    if (logLevel >= ERROR) {
      va_list args;
      va_start(args, fmt);
      vlog_va("[ERROR] ", fmt, args);
      va_end(args);
    }
  }
  void warnf(const char* fmt, ...) {
    if (logLevel >= WARN) {
      va_list args;
      va_start(args, fmt);
      vlog_va("[WARN]  ", fmt, args);
      va_end(args);
    }
  }
  void infof(const char* fmt, ...) {
    if (logLevel >= INFO) {
      va_list args;
      va_start(args, fmt);
      vlog_va("[INFO]  ", fmt, args);
      va_end(args);
    }
  }
  void debugf(const char* fmt, ...) {
    if (logLevel >= DEBUG) {
      va_list args;
      va_start(args, fmt);
      vlog_va("[DEBUG] ", fmt, args);
      va_end(args);
    }
  }

 protected:
  STM32AudioLogger() : output(&Serial), logLevel(INFO) {}
  STM32AudioLogger(const STM32AudioLogger&) = delete;
  STM32AudioLogger& operator=(const STM32AudioLogger&) = delete;

  Print* output;
  Level logLevel;

  void log(Level level, const char* prefix, const char* msg) {
    if (logLevel >= level && output) {
      output->print(prefix);
      output->println(msg);
    }
  }

  void vlog(const char* prefix, const char* fmt, ...) {
    if (!output) return;
    va_list args;
    va_start(args, fmt);
    vlog_va(prefix, fmt, args);
    va_end(args);
  }

  void vlog_va(const char* prefix, const char* fmt, va_list args) {
    if (!output) return;
    char buf[128];
    vsnprintf(buf, sizeof(buf), fmt, args);
    output->print(prefix);
    output->println(buf);
  }
};
