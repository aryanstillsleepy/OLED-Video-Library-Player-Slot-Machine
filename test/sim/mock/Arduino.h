// Minimal Arduino mock for host simulation of the sketch.
#pragma once
#include "sim.h"
#include <cctype>

#define HIGH 1
#define LOW 0
#define INPUT_PULLUP 2
#define PROGMEM
#define pgm_read_byte(p) (*(const uint8_t *)(p))

inline unsigned long micros() { return (unsigned long)(uint32_t)g_us; }
inline unsigned long millis() { return (unsigned long)(uint32_t)(g_us / 1000); }
inline void delay(unsigned long ms) { g_us += ms * 1000ULL; }
inline void pinMode(int, int) {}
inline int digitalRead(int) {
  for (auto &p : g_press) if (g_us >= p.first && g_us < p.second) return LOW;
  return HIGH;
}
inline long random(long howbig) { return howbig <= 0 ? 0 : (long)(simRand() % (uint32_t)howbig); }
inline long random(long lo, long hi) { return lo >= hi ? lo : lo + random(hi - lo); }
inline void randomSeed(unsigned long) {}  // keep old/new runs on the same sequence

class String {
  std::string s;
 public:
  String() {}
  String(const char *c) : s(c ? c : "") {}
  String(const std::string &x) : s(x) {}
  const char *c_str() const { return s.c_str(); }
  unsigned int length() const { return (unsigned int)s.size(); }
  bool endsWith(const char *suf) const {
    size_t n = strlen(suf);
    return s.size() >= n && s.compare(s.size() - n, n, suf) == 0;
  }
  bool startsWith(const char *pre) const { return s.rfind(pre, 0) == 0; }
  void remove(unsigned int idx) { if (idx < s.size()) s.erase(idx); }
  void remove(unsigned int idx, unsigned int n) { if (idx < s.size()) s.erase(idx, n); }
  void toLowerCase() { for (auto &c : s) c = (char)tolower((unsigned char)c); }
  String &operator+=(char c) { s += c; return *this; }
  String &operator+=(const String &o) { s += o.s; return *this; }
  friend String operator+(const char *a, const String &b) { return String(std::string(a) + b.s); }
};

struct SerialMock {
  void begin(long) {}
  template <typename T> void print(const T &) {}
  template <typename T> void println(const T &) {}
  void println(float, int) {}
  void println() {}
};
static SerialMock Serial;
