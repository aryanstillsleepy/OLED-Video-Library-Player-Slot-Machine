// Host simulation of the OLED_Device sketch.
//
// run_tests.py merges the sketch's .ino files (like the Arduino builder does)
// and defines SKETCH_FILE. The sketch runs against the real U8g2 C library;
// only the I2C transport is replaced, so every byte sent to the display goes
// into an SH1106 RAM model (see mock/sim.h).
//
// A scripted session drives the button through every screen. It checks the
// behaviour, writes snapshots of key screens and metrics to SIM_OUT (used to
// compare two versions of the sketch), and exits non-zero on any failure.
//
// Environment: SIM_OUT (output dir), SIM_SHOW=1 (print some screens as ASCII),
// SIM_TXN_US (assumed fixed cost per I2C transaction, default 50 us).
#include "sim.h"
#include SKETCH_FILE

#include <set>

static const uint64_t LOOP_OVERHEAD_US = 20;

static std::string g_out;
static FILE *g_metrics = nullptr;
static int g_failures = 0;
static uint64_t g_invariantFailures = 0;
static uint64_t g_maxLoopUs = 0;

static void check(bool ok, const char *what) {
  printf("  %-4s %s\n", ok ? "ok" : "FAIL", what);
  if (!ok) g_failures++;
}

static void metric(const std::string &name, double v) { fprintf(g_metrics, "%s %.3f\n", name.c_str(), v); }

// ---------------- screen capture ----------------
struct Image { std::vector<uint8_t> px; uint64_t t; };
static std::vector<Image> g_images;
static bool g_recording = false;

static std::vector<uint8_t> screen() {
  std::vector<uint8_t> v(1024);
  g_panel.visible(v.data());
  return v;
}

static void loopOnce() {
  uint64_t t0 = g_us;
  loop();
  g_us += LOOP_OVERHEAD_US;
  if (g_us - t0 > g_maxLoopUs) g_maxLoopUs = g_us - t0;

  // The panel must always show exactly what is in U8g2's buffer after a
  // loop() pass. Partial updates that miss a changed area break this.
  std::vector<uint8_t> s = screen();
  if (memcmp(s.data(), oled.getBufferPtr(), 1024) != 0 && g_invariantFailures++ < 5) {
    fprintf(stderr, "panel != buffer at t=%llu ms\n", (unsigned long long)(g_us / 1000));
  }
  if (g_recording && (g_images.empty() || g_images.back().px != s)) g_images.push_back({s, g_us});
}

static void runFor(uint64_t us) { uint64_t end = g_us + us; while (g_us < end) loopOnce(); }
static void waitMs(uint32_t ms) { runFor(ms * 1000ULL); }
static void press(uint32_t ms) { g_press.push_back({g_us, g_us + ms * 1000ULL}); runFor(ms * 1000ULL); }
static void click() { press(100); waitMs(450); }
static void doubleClick() { press(100); waitMs(120); press(100); waitMs(450); }
static void tripleClick() { press(100); waitMs(120); press(100); waitMs(120); press(100); waitMs(450); }
static void longPress() { press(800); waitMs(300); }
static void clickHold() { press(100); waitMs(120); press(900); waitMs(300); }
template <typename F> static void runUntil(F done, uint64_t maxUs) {
  uint64_t end = g_us + maxUs;
  while (!done() && g_us < end) loopOnce();
}

static void writeImage(const std::string &name, const std::vector<uint8_t> &px) {
  FILE *f = fopen((g_out + "/" + name + ".bin").c_str(), "wb");
  fwrite(px.data(), 1, px.size(), f);
  fclose(f);
}

static void snapshot(const char *name) { writeImage(name, screen()); }

static void printScreen(const char *title, int rows) {
  if (!getenv("SIM_SHOW")) return;
  std::vector<uint8_t> s = screen();
  printf("---- %s ----\n", title);
  for (int y = 0; y < rows; y++) {
    for (int x = 0; x < 128; x++) putchar(s[(y / 8) * 128 + x] & (1 << (y & 7)) ? '#' : '.');
    putchar('\n');
  }
}

// ---------------- test videos ----------------
static std::vector<std::vector<uint8_t>> g_animFrames, g_noiseFrames;

static std::vector<uint8_t> packFrame(const std::vector<uint8_t> &pix) {  // pix[y * 128 + x]
  std::vector<uint8_t> f(1024, 0);
  for (int y = 0; y < 64; y++)
    for (int x = 0; x < 128; x++)
      if (pix[y * 128 + x]) f[(y / 8) * 128 + x] |= 1 << (y & 7);
  return f;
}

static std::vector<uint8_t> makeVideo(const std::vector<std::vector<uint8_t>> &frames, uint32_t fpsMilli) {
  std::vector<uint8_t> v = {'O', 'L', 'E', 'D'};
  auto u16 = [&](uint32_t x) { v.push_back(x & 255); v.push_back(x >> 8); };
  auto u32 = [&](uint32_t x) { for (int i = 0; i < 4; i++) v.push_back((x >> (8 * i)) & 255); };
  u16(128); u16(64); u32(fpsMilli); u32((uint32_t)frames.size()); u32(1024);
  for (auto &f : frames) v.insert(v.end(), f.begin(), f.end());
  return v;
}

static void buildFs() {
  // Typical content: static border, bouncing ball, growing progress bar.
  // Every frame differs from the previous one.
  for (int i = 0; i < 90; i++) {
    std::vector<uint8_t> pix(128 * 64, 0);
    for (int x = 0; x < 128; x++) pix[x] = pix[63 * 128 + x] = 1;
    for (int y = 0; y < 64; y++) pix[y * 128] = pix[y * 128 + 127] = 1;
    int bx = 12 + (i * 3) % 100;
    int by = 14 + 20 * (1 + ((i % 30) < 15 ? (i % 15) : 15 - (i % 15))) / 16;
    for (int y = -9; y <= 9; y++)
      for (int x = -9; x <= 9; x++)
        if (x * x + y * y <= 81 && by + y > 0 && by + y < 63 && bx + x > 0 && bx + x < 127)
          pix[(by + y) * 128 + bx + x] = 1;
    for (int x = 2; x < 2 + i; x++) pix[60 * 128 + x] = 1;
    g_animFrames.push_back(packFrame(pix));
  }
  // Worst case: random noise, every byte changes every frame
  for (int i = 0; i < 40; i++) {
    std::vector<uint8_t> f(1024);
    for (auto &b : f) b = (uint8_t)simRand();
    g_noiseFrames.push_back(f);
  }
  std::vector<uint8_t> broken = makeVideo({g_noiseFrames[0]}, 30000);
  broken[0] = 'X';  // bad magic

  g_fs.push_back({"anim.bin", makeVideo(g_animFrames, 30000), false});
  g_fs.push_back({"noise.bin", makeVideo(g_noiseFrames, 200000), false});  // 200 fps: find the ceiling
  g_fs.push_back({"broken.bin", broken, false});
  g_fs.push_back({"notes.txt", {'h', 'i'}, false});
  g_fs.push_back({"A_VERY_LONG_VIDEO_FILE_NAME.BIN", makeVideo({g_animFrames[0], g_animFrames[1]}, 10000), false});
}

// Every captured image must be the next video frame (or frame 0 after a
// restart). Screens before the first frame (the library) are skipped.
struct Playback { int shown = 0, wrong = 0; double fps = 0, bytesPerFrame = 0, txPerFrame = 0; };

static Playback checkPlayback(const std::vector<std::vector<uint8_t>> &frames, uint64_t bytes0, uint64_t tx0) {
  Playback r;
  int expected = -1;
  uint64_t firstFrameTime = 0;
  for (auto &im : g_images) {
    int idx = -1;
    for (size_t k = 0; k < frames.size(); k++) if (im.px == frames[k]) { idx = (int)k; break; }
    if (expected < 0 && idx < 0) continue;
    if (expected < 0) firstFrameTime = im.t;
    if (idx < 0 || (idx != expected && idx != 0 && expected >= 0)) r.wrong++;
    expected = (idx + 1) % (int)frames.size();
    r.shown++;
  }
  if (r.shown > 0) {
    r.fps = r.shown / ((g_us - firstFrameTime) / 1e6);
    r.bytesPerFrame = (g_i2c.bytes - bytes0) / (double)r.shown;
    r.txPerFrame = (g_i2c.transactions - tx0) / (double)r.shown;
  }
  return r;
}

static void reportPlayback(const char *name, const Playback &p) {
  printf("  %-5s %d frames, %.1f fps, %.0f I2C bytes/frame, %.1f transactions/frame\n",
         name, p.shown, p.fps, p.bytesPerFrame, p.txPerFrame);
  std::string m = name;
  metric(m + "_fps", p.fps);
  metric(m + "_bytes_per_frame", p.bytesPerFrame);
  metric(m + "_tx_per_frame", p.txPerFrame);
}

int main() {
  const char *o = getenv("SIM_OUT");
  g_out = o ? o : ".";
  if (const char *t = getenv("SIM_TXN_US")) g_txnOverheadUs = atof(t);
  g_metrics = fopen((g_out + "/metrics.txt").c_str(), "w");
  if (!g_metrics) { fprintf(stderr, "cannot write to %s\n", g_out.c_str()); return 2; }

  buildFs();
  setup();
  waitMs(200);
  snapshot("01_home");

  // ---------------- video library + player ----------------
  printf("video player\n");
  doubleClick();  // Home -> Videos
  snapshot("02_library");
  printScreen("library", 50);

  g_recording = true;
  g_images.clear();
  uint64_t b0 = g_i2c.bytes, x0 = g_i2c.transactions;
  doubleClick();  // play anim.bin (30 fps)
  waitMs(3000);
  Playback anim = checkPlayback(g_animFrames, b0, x0);
  reportPlayback("anim", anim);
  check(anim.shown > 60 && anim.wrong == 0, "animation frames shown in order");

  click();  // pause
  size_t before = g_images.size();
  waitMs(1000);
  check(g_images.size() == before, "nothing changes while paused");
  click();        // resume
  doubleClick();  // restart
  waitMs(1000);
  check(checkPlayback(g_animFrames, b0, x0).wrong == 0, "frames stay in order after resume and restart");
  g_recording = false;

  longPress();  // back to library
  snapshot("03_library_back");

  click();  // select noise.bin
  doubleClick();
  waitMs(200);
  g_recording = true;
  g_images.clear();
  b0 = g_i2c.bytes;
  x0 = g_i2c.transactions;
  g_maxLoopUs = 0;
  waitMs(2000);
  Playback noise = checkPlayback(g_noiseFrames, b0, x0);
  reportPlayback("noise", noise);
  metric("noise_max_loop_ms", g_maxLoopUs / 1000.0);
  check(noise.shown > 0 && noise.wrong == 0, "worst-case video frames shown in order");
  g_recording = false;
  longPress();

  click();  // select broken.bin
  doubleClick();
  snapshot("04_broken");
  printScreen("after opening broken.bin", 12);
  longPress();  // Home

  // ---------------- placeholder screen ----------------
  printf("menus\n");
  click();        // Music
  doubleClick();  // open
  waitMs(100);
  b0 = g_i2c.bytes;
  waitMs(2000);
  metric("music_idle_bytes", (double)(g_i2c.bytes - b0));
  check(g_i2c.bytes == b0, "idle placeholder screen sends nothing");
  snapshot("05_music");
  longPress();

  // ---------------- slot machine ----------------
  printf("slot machine\n");
  click(); click(); click();  // Music -> Messages -> Settings -> Slot
  doubleClick();
  snapshot("06_slot");

  for (int spin = 0; spin < 6; spin++) {
    doubleClick();
    b0 = g_i2c.bytes;
    g_maxLoopUs = 0;
    runUntil([] { return !spinning; }, 10000000);
    if (spin == 0) {
      printf("  spin: %llu I2C bytes, longest loop() %.1f ms\n",
             (unsigned long long)(g_i2c.bytes - b0), g_maxLoopUs / 1000.0);
      metric("spin_bytes", (double)(g_i2c.bytes - b0));
      metric("spin_max_loop_ms", g_maxLoopUs / 1000.0);
    }
    runUntil([] { return !celebrating; }, 10000000);
    waitMs(100);
    char n[32];
    snprintf(n, sizeof n, "07_after_spin_%d", spin);
    snapshot(n);
  }

  clickHold();  // forced 777
  runUntil([] { return !spinning; }, 10000000);
  g_recording = true;
  g_images.clear();
  runUntil([] { return !celebrating; }, 20000000);
  waitMs(100);
  g_recording = false;
  std::set<std::vector<uint8_t>> celebration;
  for (auto &im : g_images) celebration.insert(im.px);
  int i = 0;
  for (auto &px : celebration) {
    char n[32];
    snprintf(n, sizeof n, "08_jackpot_%02d", i++);
    writeImage(n, px);
  }
  printf("  jackpot celebration: %zu distinct screens\n", celebration.size());
  snapshot("09_after_jackpot");

  tripleClick();  // history
  snapshot("10_history");
  tripleClick();  // reset prompt
  snapshot("11_reset_yes");
  click();
  snapshot("12_reset_no");
  doubleClick();  // confirm NO -> history
  snapshot("13_history_again");
  longPress();  // machine
  snapshot("14_machine");
  longPress();  // Home
  snapshot("15_home");

  // ---------------- whole session ----------------
  printf("display transport\n");
  check(g_invariantFailures == 0, "panel matched U8g2's buffer after every loop()");
  check(g_i2c.overflow == 0, "no I2C write exceeded the 128-byte Wire buffer");
  check(g_i2c.badControl == 0, "every I2C write started with a valid control byte");
  printf("  %llu I2C transactions, largest payload %llu bytes\n",
         (unsigned long long)g_i2c.transactions, (unsigned long long)g_i2c.maxPayload);

  fclose(g_metrics);
  printf("simulation: %s\n", g_failures ? "FAILED" : "passed");
  return g_failures ? 1 : 0;
}
