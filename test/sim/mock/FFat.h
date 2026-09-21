#pragma once
#include "Arduino.h"
#define FILE_READ "r"

class File {
  int idx = -2;  // -2 = invalid, -1 = root directory, >= 0 = g_fs index
  size_t pos = 0;
  size_t dirIter = 0;
 public:
  File() {}
  explicit File(int i) : idx(i) {}
  explicit operator bool() const { return idx != -2; }
  bool isDirectory() const { return idx == -1 || (idx >= 0 && g_fs[idx].dir); }
  const char *name() const { return idx >= 0 ? g_fs[idx].name.c_str() : "/"; }
  File openNextFile() {
    if (idx != -1 || dirIter >= g_fs.size()) return File();
    return File((int)dirIter++);
  }
  void close() { idx = -2; }
  size_t size() const { return idx >= 0 ? g_fs[idx].data.size() : 0; }
  size_t read(uint8_t *buf, size_t n) {
    if (idx < 0) return 0;
    const auto &d = g_fs[idx].data;
    size_t avail = pos < d.size() ? d.size() - pos : 0;
    if (n > avail) n = avail;
    memcpy(buf, d.data() + pos, n);
    pos += n;
    return n;
  }
  bool seek(uint32_t p) { if (idx < 0 || p > size()) return false; pos = p; return true; }
};

struct FFatMock {
  bool begin(bool) { return true; }
  File open(const char *path, const char * = FILE_READ) {
    std::string p(path);
    if (!p.empty() && p[0] == '/') p.erase(0, 1);
    if (p.empty()) return File(-1);
    for (size_t i = 0; i < g_fs.size(); i++) if (g_fs[i].name == p) return File((int)i);
    return File();
  }
  File open(const String &path, const char *mode = FILE_READ) { return open(path.c_str(), mode); }
};
static FFatMock FFat;
