// Host test for OLED_Device/Button.ino.
//
// Compiles the real Button.ino against a mocked millis()/digitalRead() and
// replays button timelines, with and without contact bounce, at several
// loop() periods. run_tests.py defines BUTTON_FILE and adds the sketch folder
// to the include path (for Config.h).
//
// Loop periods: 1 ms is an idle loop, 14 ms is the longest loop() pass of the
// firmware (a full-screen video frame at 800 kHz), 20 ms leaves headroom. The
// debounce needs the input stable for DEBOUNCE_MS between two samples, so very
// short taps are missed once loop() takes longer than about 22 ms.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <utility>

#define HIGH 1
#define LOW 0
#define INPUT_PULLUP 2

static unsigned long g_now = 0;
static std::vector<std::pair<unsigned long, unsigned long>> g_presses;  // [start, end) ms
static std::vector<std::pair<unsigned long, int>> g_bounces;            // forced raw levels

unsigned long millis() { return g_now; }
void pinMode(int, int) {}
int digitalRead(int) {
  for (auto &b : g_bounces) if (b.first == g_now) return b.second;
  for (auto &p : g_presses) if (g_now >= p.first && g_now < p.second) return LOW;
  return HIGH;
}

#include "Config.h"

enum AppState { STATE_HOME, STATE_VIDEOS, STATE_PLAYER, STATE_MUSIC, STATE_MESSAGES, STATE_SETTINGS, STATE_SLOT };
AppState currentState = STATE_SLOT;  // the slot screen uses every gesture

static std::vector<std::string> g_events;
void handleSlotSingleClick() { g_events.push_back("single"); }
void handleSlotDoubleClick() { g_events.push_back("double"); }
void handleSlotTripleClick() { g_events.push_back("triple"); }
void handleSlotLongPress() { g_events.push_back("long"); }
void handleSlotClickHold() { g_events.push_back("clickhold"); }

// Handlers for other screens, referenced by Button.ino
void handleHomeClick() {} void openHomeSelection() {} void handleVideoLibraryClick() {}
void openSelectedVideo() {} void handlePlayerClick() {} void restartVideo() {}
void handleMusicClick() {} void handleMusicDoubleClick() {} void handleMessagesClick() {}
void handleMessagesDoubleClick() {} void handleSettingsClick() {} void handleSettingsDoubleClick() {}
void handleVideoLibraryTripleClick() {} void handlePlayerTripleClick() {}
void drawHome() {} void drawLibrary() {} void closeVideo() {}

// Prototypes the Arduino builder would generate for Button.ino
void processClicks(); void handleSingleClick(); void handleDoubleClick(); void handleTripleClick();
void handleLongPress(); void handleClickHold(); void handleHomeTripleClick();
void handleMusicTripleClick(); void handleMessagesTripleClick(); void handleSettingsTripleClick();

#include BUTTON_FILE

struct Case {
  const char *name;
  std::vector<std::pair<unsigned long, unsigned long>> presses;
  const char *expect;
};

static std::string joined() {
  std::string s;
  for (auto &e : g_events) s += (s.empty() ? "" : ",") + e;
  return s;
}

int main() {
  std::vector<Case> cases = {
    {"quick single click",   {{100, 180}},                         "single"},
    {"fast double click",    {{100, 180}, {260, 340}},             "double"},
    {"slow double click",    {{100, 200}, {350, 560}},             "double"},
    {"triple click",         {{100, 170}, {240, 310}, {380, 450}}, "triple"},
    {"long press",           {{100, 1000}},                        "long"},
    {"click + hold",         {{100, 180}, {300, 1100}},            "clickhold"},
    {"two separate singles", {{100, 180}, {900, 980}},             "single,single"},
  };
  const int loopPeriods[] = {1, 14, 20};

  int total = 0, fails = 0;
  for (int bounce = 0; bounce < 2; bounce++) {
    for (int period : loopPeriods) {
      for (auto &c : cases) {
        g_presses = c.presses;
        g_bounces.clear();
        g_events.clear();
        if (bounce) {
          for (auto &p : c.presses) {  // 2 ms of chatter on each edge
            g_bounces.push_back({p.first + 1, HIGH});
            g_bounces.push_back({p.first + 2, LOW});
            g_bounces.push_back({p.second + 1, LOW});
            g_bounces.push_back({p.second + 2, HIGH});
          }
        }
        g_now = 0;
        setupButton();
        for (g_now = 0; g_now < 2500; g_now++) {
          if (g_now % period == 0) updateButton();
        }
        std::string got = joined();
        total++;
        if (got != c.expect) {
          fails++;
          printf("FAIL loop=%2d ms bounce=%d %-22s expected=%s got=%s\n", period, bounce, c.name, c.expect, got.c_str());
        }
      }
    }
  }
  printf("button: %d/%d cases passed\n", total - fails, total);
  return fails ? 1 : 0;
}
