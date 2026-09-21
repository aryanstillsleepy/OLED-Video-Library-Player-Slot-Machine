// =====================================================
// VIDEOS
// VIDEO LIBRARY + VIDEO PLAYER
// =====================================================

// =====================================================
// VIDEO DATA
// =====================================================

uint8_t frameBuffer[FRAME_SIZE];
uint8_t xbmBuffer[FRAME_SIZE];

File videoFile;

float videoFPS = 30.0;

uint32_t frameCount = 0;
uint32_t currentFrame = 0;

bool videoPlaying = false;
bool videoPaused = false;

uint32_t nextFrameTime = 0;


// =====================================================
// VIDEO LIBRARY
// =====================================================

String videoNames[MAX_VIDEOS];

int videoCount = 0;
int selectedVideo = 0;
int scrollOffset = 0;


// =====================================================
// DRAW LIBRARY
// =====================================================

void drawLibrary() {

  oled.clearBuffer();

  oled.setFont(u8g2_font_6x10_tf);

  oled.drawStr(0, 9, "VIDEO LIBRARY");

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

    if (displayName.endsWith(".bin")) {
      displayName.remove(displayName.length() - 4);
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

      if (name.endsWith(".bin")) {

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

  if (videoCount <= VISIBLE_VIDEOS) {

    scrollOffset = 0;
  }

  else if (
    scrollOffset >
    videoCount - VISIBLE_VIDEOS
  ) {

    scrollOffset =
      videoCount - VISIBLE_VIDEOS;
  }
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

  videoFPS =
    (float)fpsMilli / 1000.0f;

  Serial.println();
  Serial.println("VIDEO HEADER");
  Serial.println("------------------------------");

  Serial.print("Resolution: ");
  Serial.print(width);
  Serial.print(" x ");
  Serial.println(height);

  Serial.print("FPS: ");
  Serial.println(
    videoFPS,
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
    videoFPS <= 0 ||
    frameCount == 0
  ) {

    Serial.println(
      "ERROR: Invalid FPS or frame count"
    );

    return false;
  }

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
// CONVERT FRAME TO XBM
// =====================================================

void convertFrameToXBM() {

  memset(
    xbmBuffer,
    0,
    FRAME_SIZE
  );

  for (
    int page = 0;
    page < 8;
    page++
  ) {

    for (
      int x = 0;
      x < 128;
      x++
    ) {

      uint8_t sourceByte =
        frameBuffer[
          page * 128 + x
        ];

      for (
        int bit = 0;
        bit < 8;
        bit++
      ) {

        int y =
          page * 8 + bit;

        int pixelIndex =
          y * 128 + x;

        int byteIndex =
          pixelIndex / 8;

        int bitIndex =
          pixelIndex % 8;

        if (
          sourceByte &
          (1 << bit)
        ) {

          xbmBuffer[byteIndex] |=
            (1 << bitIndex);
        }
      }
    }
  }
}


// =====================================================
// RENDER FRAME
// =====================================================

void renderFrame() {

  convertFrameToXBM();

  oled.clearBuffer();

  oled.drawXBM(
    0,
    0,
    FRAME_WIDTH,
    FRAME_HEIGHT,
    xbmBuffer
  );

  oled.sendBuffer();
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

  oled.clearBuffer();
  oled.sendBuffer();
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

  uint32_t frameIntervalUs =
    (uint32_t)(
      1000000.0 /
      videoFPS
    );

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

  renderFrame();

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
    scrollOffset = 0;
  }

  // Keep selected item visible

  if (
    selectedVideo >=
    scrollOffset +
    VISIBLE_VIDEOS
  ) {

    scrollOffset++;
  }

  if (
    selectedVideo <
    scrollOffset
  ) {

    scrollOffset =
      selectedVideo;
  }

  if (selectedVideo == 0) {
    scrollOffset = 0;
  }

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

    currentState =
      STATE_PLAYER;
  }
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