#pragma once
#include <Arduino.h>

class Logger {
 public:
  enum Level {
    LOG_NONE = 0,
    LOG_ERROR = 1,
    LOG_WARN = 2,
    LOG_INFO = 3,
    LOG_DEBUG = 4
  };

  static Logger& instance() {
    static Logger _instance;
    return _instance;
  }

  void begin(Level level, Print& out = Serial) {
    setLevel(level);
    setOutput(out);
  }

  void setOutput(Print& out) { output = &out; }
  void setLevel(Level level) { logLevel = level; }
  Level getLevel() const { return logLevel; }

  void error(const char* msg) { log(LOG_ERROR, "[ERROR] ", msg); }
  void warn(const char* msg) { log(LOG_WARN, "[WARN]  ", msg); }
  void info(const char* msg) { log(LOG_INFO, "[INFO]  ", msg); }
  void debug(const char* msg) { log(LOG_DEBUG, "[DEBUG] ", msg); }

  void errorf(const char* fmt, ...) {
    if (logLevel >= LOG_ERROR) {
      va_list args;
      va_start(args, fmt);
      vlog_va("[ERROR] ", fmt, args);
      va_end(args);
    }
  }
  void warnf(const char* fmt, ...) {
    if (logLevel >= LOG_WARN) {
      va_list args;
      va_start(args, fmt);
      vlog_va("[WARN]  ", fmt, args);
      va_end(args);
    }
  }
  void infof(const char* fmt, ...) {
    if (logLevel >= LOG_INFO) {
      va_list args;
      va_start(args, fmt);
      vlog_va("[INFO]  ", fmt, args);
      va_end(args);
    }
  }
  void debugf(const char* fmt, ...) {
    if (logLevel >= LOG_DEBUG) {
      va_list args;
      va_start(args, fmt);
      vlog_va("[DEBUG] ", fmt, args);
      va_end(args);
    }
  }

 protected:
  Logger() : output(&Serial), logLevel(LOG_INFO) {}
  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;

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
