#pragma once
#include "Arduino.h"
class Preferences {
  std::map<std::string, int> kv;
 public:
  bool begin(const char *, bool) { return true; }
  int getInt(const char *k, int def) { auto it = kv.find(k); return it == kv.end() ? def : it->second; }
  size_t putInt(const char *k, int v) { kv[k] = v; return 4; }
};
