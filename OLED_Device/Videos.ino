// =====================================================
// VIDEOS
// VIDEO LIBRARY + VIDEO PLAYER
// =====================================================

// =====================================================
// VIDEO DATA
// =====================================================

// Frame read from the file, in the display's page layout
uint8_t frameBuffer[FRAME_SIZE];

File videoFile;

uint32_t frameCount = 0;
uint32_t currentFrame = 0;

bool videoPlaying = false;
bool videoPaused = false;

uint32_t nextFrameTime = 0;

// From the header's fps x 1000, computed once per video
uint32_t frameIntervalUs = 33333;

// The screen shows something other than the last
// frame (e.g. the library), so send the next one whole
bool fullRedrawNeeded = true;

// 8x8 pixel tiles per page (a page is 8 pixel rows)
const uint8_t FRAME_TILES_X = FRAME_WIDTH / 8;
const uint8_t FRAME_PAGES = FRAME_HEIGHT / 8;

// Unchanged tiles bridged between two changed ones in a
// single transfer. Starting a new transfer costs about
// as much as sending two tiles.
const uint8_t TILE_MERGE_GAP = 2;


// =====================================================
// VIDEO LIBRARY
// =====================================================

String videoNames[MAX_VIDEOS];

int videoCount = 0;
int selectedVideo = 0;
int scrollOffset = 0;

// Shown instead of the title after a failed open.
// Cleared on the next library action.
const char* libraryError = nullptr;

// Names are drawn from x = 10 with a 6 px font and
// must stop before the scroll bar at x = 125.
const int MAX_NAME_CHARS = 19;


// =====================================================
// KEEP SELECTION VISIBLE
// =====================================================

void keepSelectionVisible() {

  if (selectedVideo < scrollOffset) {
    scrollOffset = selectedVideo;
  }

  if (selectedVideo >= scrollOffset + VISIBLE_VIDEOS) {
    scrollOffset = selectedVideo - VISIBLE_VIDEOS + 1;
  }

  int maxScroll = videoCount - VISIBLE_VIDEOS;

  if (maxScroll < 0) {
    maxScroll = 0;
  }

  if (scrollOffset > maxScroll) {
    scrollOffset = maxScroll;
  }

  if (scrollOffset < 0) {
    scrollOffset = 0;
  }
}


// =====================================================
// DRAW LIBRARY
// =====================================================

void drawLibrary() {

  oled.clearBuffer();

  oled.setFont(u8g2_font_6x10_tf);

  oled.drawStr(
    0,
    9,
    libraryError ? libraryError : "VIDEO LIBRARY"
  );

  if (videoCount == 0) {

    oled.drawStr(0, 30, "No videos");

    oled.sendBuffer();

    return;
  }

  for (int i = 0; i < VISIBLE_VIDEOS; i++) {

    int videoIndex = scrollOffset + i;

    if (videoIndex >= videoCount) {
      break;
    }

    int y = 20 + i * 9;

    if (videoIndex == selectedVideo) {
      oled.drawStr(0, y, ">");
    }

    // -------------------------------------------------
    // Hide .bin from display
    // -------------------------------------------------

    String displayName = videoNames[videoIndex];

    // scanVideos() only keeps names ending in .bin
    displayName.remove(displayName.length() - 4);

    // Keep long names clear of the scroll bar
    if ((int)displayName.length() > MAX_NAME_CHARS) {
      displayName.remove(MAX_NAME_CHARS - 1);
      displayName += '~';
    }

    oled.drawStr(
      10,
      y,
      displayName.c_str()
    );
  }

  // ===================================================
  // SCROLL BAR
  // ===================================================

  if (videoCount > VISIBLE_VIDEOS) {

    int trackHeight = 43;

    int maxScroll =
      videoCount - VISIBLE_VIDEOS;

    int thumbHeight =
      (VISIBLE_VIDEOS * trackHeight) / videoCount;

    if (thumbHeight < 5) {
      thumbHeight = 5;
    }

    int thumbPosition = 0;

    if (maxScroll > 0) {

      thumbPosition =
        ((trackHeight - thumbHeight) * scrollOffset)
        / maxScroll;
    }

    oled.drawBox(
      125,
      18 + thumbPosition,
      2,
      thumbHeight
    );
  }

  oled.sendBuffer();
}


// =====================================================
// SCAN VIDEOS
// =====================================================

void scanVideos() {

  videoCount = 0;

  File root = FFat.open("/");

  if (!root) {

    Serial.println(
      "Failed to open FFAT root"
    );

    return;
  }

  File file = root.openNextFile();

  while (
    file &&
    videoCount < MAX_VIDEOS
  ) {

    if (!file.isDirectory()) {

      String name = file.name();

      String lowerName = name;
      lowerName.toLowerCase();

      // FAT tools often store 8.3 names in upper case
      if (lowerName.endsWith(".bin")) {

        if (name.startsWith("/")) {
          name.remove(0, 1);
        }

        videoNames[videoCount] =
          name;

        Serial.print(
          "Found video: "
        );

        Serial.println(name);

        videoCount++;
      }
    }

    file.close();

    file = root.openNextFile();
  }

  root.close();

  Serial.print(
    "Total videos: "
  );

  Serial.println(videoCount);

  if (selectedVideo >= videoCount) {
    selectedVideo = 0;
  }

  // Files may have been removed since the last scan
  keepSelectionVisible();

  libraryError = nullptr;
}


// =====================================================
// READ UINT16
// =====================================================

uint16_t readUInt16(uint8_t *p) {

  return
    ((uint16_t)p[0]) |
    ((uint16_t)p[1] << 8);
}


// =====================================================
// READ UINT32
// =====================================================

uint32_t readUInt32(uint8_t *p) {

  return
    ((uint32_t)p[0]) |
    ((uint32_t)p[1] << 8) |
    ((uint32_t)p[2] << 16) |
    ((uint32_t)p[3] << 24);
}


// =====================================================
// READ VIDEO HEADER
// =====================================================

bool readVideoHeader(File &file) {

  uint8_t header[VIDEO_HEADER_SIZE];

  if (
    file.read(
      header,
      VIDEO_HEADER_SIZE
    ) != VIDEO_HEADER_SIZE
  ) {

    return false;
  }

  // ===================================================
  // CHECK MAGIC
  // ===================================================

  if (
    header[0] != 'O' ||
    header[1] != 'L' ||
    header[2] != 'E' ||
    header[3] != 'D'
  ) {

    return false;
  }

  uint16_t width =
    readUInt16(&header[4]);

  uint16_t height =
    readUInt16(&header[6]);

  uint32_t fpsMilli =
    readUInt32(&header[8]);

  frameCount =
    readUInt32(&header[12]);

  uint32_t frameSize =
    readUInt32(&header[16]);

  Serial.println();
  Serial.println("VIDEO HEADER");
  Serial.println("------------------------------");

  Serial.print("Resolution: ");
  Serial.print(width);
  Serial.print(" x ");
  Serial.println(height);

  Serial.print("FPS: ");
  Serial.println(
    fpsMilli / 1000.0f,
    3
  );

  Serial.print("Frames: ");
  Serial.println(frameCount);

  Serial.print("Frame size: ");
  Serial.println(frameSize);

  Serial.println("------------------------------");

  // ===================================================
  // VALIDATION
  // ===================================================

  if (
    width != FRAME_WIDTH ||
    height != FRAME_HEIGHT
  ) {

    Serial.println(
      "ERROR: Invalid resolution"
    );

    return false;
  }

  if (frameSize != FRAME_SIZE) {

    Serial.println(
      "ERROR: Invalid frame size"
    );

    return false;
  }

  if (
    fpsMilli == 0 ||
    frameCount == 0
  ) {

    Serial.println(
      "ERROR: Invalid FPS or frame count"
    );

    return false;
  }

  frameIntervalUs =
    (uint32_t)(1000000000ULL / fpsMilli);

  uint64_t expectedSize =
    VIDEO_HEADER_SIZE +
    (
      (uint64_t)FRAME_SIZE *
      frameCount
    );

  uint64_t actualSize =
    file.size();

  Serial.print(
    "Expected file size: "
  );

  Serial.println(
    (uint32_t)expectedSize
  );

  Serial.print(
    "Actual file size:   "
  );

  Serial.println(
    (uint32_t)actualSize
  );

  if (
    actualSize != expectedSize
  ) {

    Serial.println(
      "ERROR: File size mismatch"
    );

    return false;
  }

  return true;
}


// =====================================================
// FRAME TILE CHANGED
// =====================================================

bool frameTileChanged(
  const uint8_t *frameRow,
  const uint8_t *screenRow,
  uint8_t tile
) {

  return memcmp(
    frameRow + tile * 8,
    screenRow + tile * 8,
    8
  ) != 0;
}


// =====================================================
// PRESENT FRAME
// =====================================================
//
// Video frames use the same page layout as U8g2's frame
// buffer, so they need no conversion.
//
// The U8g2 buffer always holds what is on screen, so
// only the 8x8 tiles that differ from it are copied in
// and sent. Static parts of a video cost nothing, and a
// frame that changes everywhere costs the same as a
// full sendBuffer().

void presentFrame() {

  uint8_t *screen = oled.getBufferPtr();

  if (fullRedrawNeeded) {

    memcpy(
      screen,
      frameBuffer,
      FRAME_SIZE
    );

    oled.sendBuffer();

    fullRedrawNeeded = false;

    return;
  }

  for (
    uint8_t page = 0;
    page < FRAME_PAGES;
    page++
  ) {

    const uint8_t *frameRow =
      frameBuffer + page * FRAME_WIDTH;

    uint8_t *screenRow =
      screen + page * FRAME_WIDTH;

    uint8_t tile = 0;

    while (tile < FRAME_TILES_X) {

      if (!frameTileChanged(frameRow, screenRow, tile)) {
        tile++;
        continue;
      }

      uint8_t first = tile;
      uint8_t last = tile;

      // Extend the run across short unchanged gaps
      for (
        tile++;
        tile < FRAME_TILES_X &&
        tile - last <= TILE_MERGE_GAP;
        tile++
      ) {

        if (frameTileChanged(frameRow, screenRow, tile)) {
          last = tile;
        }
      }

      uint8_t count = last - first + 1;

      memcpy(
        screenRow + first * 8,
        frameRow + first * 8,
        count * 8
      );

      oled.updateDisplayArea(
        first,
        page,
        count,
        1
      );
    }
  }
}


// =====================================================
// OPEN VIDEO
// =====================================================

bool openVideo(int index) {

  if (
    index < 0 ||
    index >= videoCount
  ) {

    return false;
  }

  if (videoFile) {
    videoFile.close();
  }

  String path =
    "/" + videoNames[index];

  Serial.println();

  Serial.print(
    "Opening: "
  );

  Serial.println(
    videoNames[index]
  );

  videoFile =
    FFat.open(
      path,
      FILE_READ
    );

  if (!videoFile) {

    Serial.println(
      "ERROR: Failed to open file"
    );

    return false;
  }

  if (
    !readVideoHeader(
      videoFile
    )
  ) {

    videoFile.close();

    Serial.println(
      "ERROR: Invalid video"
    );

    return false;
  }

  currentFrame = 0;

  videoPlaying = true;
  videoPaused = false;

  fullRedrawNeeded = true;

  nextFrameTime = micros();

  Serial.println(
    "VIDEO PLAYBACK STARTED"
  );

  return true;
}


// =====================================================
// CLOSE VIDEO
// =====================================================

void closeVideo() {

  videoPlaying = false;
  videoPaused = false;

  if (videoFile) {

    videoFile.close();

    Serial.println(
      "Video closed"
    );
  }

  // No screen clear here: the caller draws the library
  // right away, and a blank frame in between only costs
  // a full transfer and flickers.
}


// =====================================================
// RESTART VIDEO
// =====================================================

void restartVideo() {

  if (!videoFile) {
    return;
  }

  videoFile.seek(
    VIDEO_HEADER_SIZE
  );

  currentFrame = 0;

  videoPlaying = true;
  videoPaused = false;

  nextFrameTime = micros();
}


// =====================================================
// UPDATE VIDEO
// =====================================================

void updateVideo() {

  if (!videoPlaying) {
    return;
  }

  if (videoPaused) {
    return;
  }

  if (!videoFile) {
    return;
  }

  uint32_t now = micros();

  if (
    (int32_t)(
      now -
      nextFrameTime
    ) < 0
  ) {

    return;
  }

  // ===================================================
  // VIDEO FINISHED
  // LOOP
  // ===================================================

  if (
    currentFrame >=
    frameCount
  ) {

    currentFrame = 0;

    videoFile.seek(
      VIDEO_HEADER_SIZE
    );

    nextFrameTime =
      micros();

    return;
  }

  // ===================================================
  // READ FRAME
  // ===================================================

  size_t bytesRead =
    videoFile.read(
      frameBuffer,
      FRAME_SIZE
    );

  if (
    bytesRead != FRAME_SIZE
  ) {

    Serial.println(
      "Frame read failed - restarting video"
    );

    videoFile.seek(
      VIDEO_HEADER_SIZE
    );

    currentFrame = 0;

    nextFrameTime =
      micros();

    return;
  }

  // ===================================================
  // DISPLAY FRAME
  // ===================================================

  presentFrame();

  currentFrame++;

  nextFrameTime +=
    frameIntervalUs;

  // Prevent large timing drift

  if (
    (int32_t)(
      now -
      nextFrameTime
    ) > 0
  ) {

    nextFrameTime =
      now +
      frameIntervalUs;
  }
}


// =====================================================
// VIDEO LIBRARY SINGLE CLICK
// =====================================================
//
// Called by Button.ino
// =====================================================

void handleVideoLibraryClick() {

  if (videoCount <= 0) {
    return;
  }

  selectedVideo++;

  if (
    selectedVideo >=
    videoCount
  ) {

    selectedVideo = 0;
  }

  keepSelectionVisible();

  libraryError = nullptr;

  drawLibrary();
}


// =====================================================
// OPEN SELECTED VIDEO
// =====================================================
//
// Called by Button.ino
// =====================================================

void openSelectedVideo() {

  if (videoCount <= 0) {
    return;
  }

  if (
    openVideo(
      selectedVideo
    )
  ) {

    libraryError = nullptr;

    currentState =
      STATE_PLAYER;

    return;
  }

  // Tell the user, otherwise it looks like the
  // button stopped working
  libraryError = "! INVALID VIDEO";

  drawLibrary();
}


// =====================================================
// VIDEO LIBRARY TRIPLE CLICK
// =====================================================

void handleVideoLibraryTripleClick() {

  // Reserved for future function
}


// =====================================================
// PLAYER SINGLE CLICK
// =====================================================
//
// Pause / Resume
// =====================================================

void handlePlayerClick() {

  if (!videoPlaying) {
    return;
  }

  videoPaused =
    !videoPaused;

  if (!videoPaused) {

    nextFrameTime =
      micros();
  }
}


// =====================================================
// PLAYER TRIPLE CLICK
// =====================================================

void handlePlayerTripleClick() {

  // Reserved for future function
}


// =====================================================
// UPDATE VIDEO LIBRARY
// =====================================================

void updateVideos() {

  // Library is event-driven.
  // Button actions redraw the screen when needed.

}


// =====================================================
// UPDATE VIDEO PLAYER
// =====================================================

void updatePlayer() {

  updateVideo();
}