// Shared simulator state: clock, button, I2C capture, SH1106 model.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <utility>

extern "C" {
#include "u8g2.h"
}

// ---------------- time ----------------
static uint64_t g_us = 0;

// ---------------- button ----------------
static std::vector<std::pair<uint64_t, uint64_t>> g_press;  // [start, end) in us

// ---------------- I2C stats ----------------
struct I2cStats {
  uint64_t transactions = 0;
  uint64_t bytes = 0;       // incl. address byte
  uint64_t overflow = 0;    // bytes dropped because Wire's 128-byte buffer was full
  uint64_t badControl = 0;  // transaction not starting with 0x00 / 0x40
  uint64_t maxPayload = 0;
};
static I2cStats g_i2c;
static double g_txnOverheadUs = 50.0;  // fixed cost per I2C transaction (assumed)

// ---------------- SH1106 model ----------------
struct SH1106 {
  uint8_t ram[8][132];
  int page = 0, col = 0;
  bool pendingArg = false;
  SH1106() { memset(ram, 0xA5, sizeof ram); }  // garbage at power-up
  void cmd(uint8_t c) {
    if (pendingArg) { pendingArg = false; return; }
    if (c >= 0xB0 && c <= 0xB7) page = c & 7;
    else if (c <= 0x0F) col = (col & 0xF0) | c;
    else if (c <= 0x1F) col = ((c & 0x0F) << 4) | (col & 0x0F);
    else if (c == 0x81 || c == 0xA8 || c == 0xD3 || c == 0xD5 || c == 0xD9 ||
             c == 0xDA || c == 0xDB || c == 0xAD || c == 0x8D || c == 0x20)
      pendingArg = true;
  }
  void receive(const std::vector<uint8_t> &t) {
    if (t.empty()) return;
    if (t[0] == 0x00) {
      for (size_t i = 1; i < t.size(); i++) cmd(t[i]);
    } else if (t[0] == 0x40) {
      for (size_t i = 1; i < t.size(); i++) {
        if (col < 132) ram[page][col] = t[i];
        col++;
      }
    } else {
      g_i2c.badControl++;
    }
  }
  // Visible 128x64 area in page layout (x offset 2 for the 132-column SH1106)
  void visible(uint8_t *out) const {
    for (int p = 0; p < 8; p++) memcpy(out + p * 128, &ram[p][2], 128);
  }
};
static SH1106 g_panel;

static std::vector<uint8_t> g_tx;
static bool g_inTx = false;

extern "C" uint8_t sim_byte_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
  switch (msg) {
    case U8X8_MSG_BYTE_SEND: {
      uint8_t *d = (uint8_t *)arg_ptr;
      for (int i = 0; i < arg_int; i++) {
        if (g_tx.size() >= 128) g_i2c.overflow++;  // ESP32 Wire::write drops it
        else g_tx.push_back(d[i]);
      }
      break;
    }
    case U8X8_MSG_BYTE_INIT:
      if (u8x8->bus_clock == 0) u8x8->bus_clock = u8x8->display_info->i2c_bus_clock_100kHz * 100000UL;
      break;
    case U8X8_MSG_BYTE_START_TRANSFER:
      g_tx.clear();
      g_inTx = true;
      break;
    case U8X8_MSG_BYTE_END_TRANSFER: {
      g_panel.receive(g_tx);
      size_t wire = g_tx.size() + 1;  // + address byte
      g_i2c.transactions++;
      g_i2c.bytes += wire;
      if (g_tx.size() > g_i2c.maxPayload) g_i2c.maxPayload = g_tx.size();
      double us = wire * 9.0 * 1e6 / (double)u8x8->bus_clock + g_txnOverheadUs;
      g_us += (uint64_t)(us + 0.5);
      g_inTx = false;
      break;
    }
    default:
      break;
  }
  return 1;
}

extern "C" uint8_t sim_gpio_cb(u8x8_t *, uint8_t msg, uint8_t arg_int, void *) {
  if (msg == U8X8_MSG_DELAY_MILLI) g_us += arg_int * 1000ULL;
  else if (msg == U8X8_MSG_DELAY_10MICRO) g_us += arg_int * 10ULL;
  return 1;
}

// ---------------- in-memory FFat ----------------
struct SimFile { std::string name; std::vector<uint8_t> data; bool dir; };
static std::vector<SimFile> g_fs;

// ---------------- deterministic random ----------------
static uint32_t g_rng = 0x12345678u;
static uint32_t simRand() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
