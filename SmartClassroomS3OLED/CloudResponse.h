#pragma once
#include <Arduino.h>

// HTTPClient decodes chunked/unknown-length bodies into this bounded sink.
// Never accumulate an unbounded response or expose Firebase tokens in logs.
class CloudResponse : public Stream {
 public:
  static constexpr size_t limit = 16384;
  String body;
  bool overflow = false, allocationFailed = false;
  size_t write(uint8_t value) override { return write(&value, 1); }
  size_t write(const uint8_t *data, size_t length) override {
    if (overflow || allocationFailed) return 0;
    if (length > limit - body.length()) { overflow = true; return 0; }
    if (!body.concat(reinterpret_cast<const char *>(data), length)) {
      allocationFailed = true; return 0;
    }
    return length;
  }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}
};
