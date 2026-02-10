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
    if (logLevel >= LOG_ERROR) vlog("[ERROR] ", fmt);
  }
  void warnf(const char* fmt, ...) {
    if (logLevel >= LOG_WARN) vlog("[WARN]  ", fmt);
  }
  void infof(const char* fmt, ...) {
    if (logLevel >= LOG_INFO) vlog("[INFO]  ", fmt);
  }
  void debugf(const char* fmt, ...) {
    if (logLevel >= LOG_DEBUG) vlog("[DEBUG] ", fmt);
  }

 private:
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
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    output->print(prefix);
    output->println(buf);
  }
};
