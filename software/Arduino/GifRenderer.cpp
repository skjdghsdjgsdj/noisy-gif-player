#include "GifRenderer.h"
#include "I2SWavPlayer.h"
#include <SD_MMC.h>
#include "esp_timer.h"

GifRenderer::GifRenderer()
  : tft(NULL), decodeBuffer(0), skipFrame(false),
    prevFrameWasOpaque(false), currentFrameHasTransparency(false),
    displayQueue(NULL), displayWriterHandle(NULL) {
  bufferFree[0] = NULL;
  bufferFree[1] = NULL;
}

GifRenderer &GifRenderer::instance() {
  static GifRenderer inst;
  return inst;
}

void GifRenderer::begin(Adafruit_ST7789 &display) {
  tft = &display;
  // BIG_ENDIAN_PIXELS stores palette colors in the byte order the ST7789 expects
  // over SPI (high byte first). writePixels(..., bigEndian=true) then passes the
  // framebuffer directly to DMA without any per-pixel byte-swap pass.
  gif.begin(BIG_ENDIAN_PIXELS);
  clearFrameBuffer();

  displayQueue  = xQueueCreate(1, sizeof(int));
  bufferFree[0] = xSemaphoreCreateBinary();
  bufferFree[1] = xSemaphoreCreateBinary();
  xSemaphoreGive(bufferFree[0]);
  xSemaphoreGive(bufferFree[1]);
}

bool GifRenderer::playGif(const String &gifPath) {
  if (!gif.open(
        gifPath.c_str(),
        GIFOpenFile,
        GIFCloseFile,
        GIFReadFile,
        GIFSeekFile,
        GIFDraw
      )) {
    return false;
  }

  startDisplayWriterTask();

  I2SWavPlayer &player = I2SWavPlayer::instance();
  bool haveAudio = player.isActive();

  // Use the audio render start time as the GIF timeline origin so that
  // frame 0 is displayed exactly when the first audio sample is heard,
  // eliminating the fixed phase offset caused by I2S DMA pipeline latency.
  // Without audio, fall back to the current wall clock.
  uint64_t startUs     = haveAudio ? player.getAudioRenderStartUs()
                                   : esp_timer_get_time();
  uint64_t nextFrameUs = startUs;  // deadline for frame 0

  int  lastPosted    = -1;
  bool playing       = true;
  bool dropNextFrame = false;

  while (playing) {
    // Sleep until this frame's deadline, overlapping with the DisplayWriter's
    // SPI transfer for the previous frame. If we're already behind, skip sleep.
    int64_t sleepUs = (int64_t)nextFrameUs - (int64_t)esp_timer_get_time();
    if (sleepUs >= 1000) {
      vTaskDelay(pdMS_TO_TICKS(sleepUs / 1000));
    } else if (sleepUs > 0) {
      esp_rom_delay_us((uint32_t)sleepUs);
    }

    xSemaphoreTake(bufferFree[decodeBuffer], portMAX_DELAY);

    // Carry forward the previous frame as the background for transparent pixels.
    // Safe: DisplayWriter may still be reading frameBuffer[decodeBuffer ^ 1] via SPI
    // DMA, but we only read it here too — concurrent reads have no data race.
    //
    // Skip the copy when the previous frame was fully opaque: every pixel in the
    // decode buffer will be overwritten regardless, so copying is pure waste.
    // For full-motion video GIFs this is almost always the case, saving a 64 KB
    // memcpy per frame (~0.3 ms each on internal SRAM).
    if (!prevFrameWasOpaque) {
      memcpy(frameBuffer[decodeBuffer], frameBuffer[decodeBuffer ^ 1],
             sizeof(frameBuffer[0]));
    }

    // Reset per-frame transparency flag; draw() sets it if any scanline is transparent.
    currentFrameHasTransparency = false;

    // skipFrame is read inside flushFrameIfLastLine() (called from gif.playFrame).
    // When true, that callback releases bufferFree[decodeBuffer] itself instead of
    // posting to the display queue, so the DisplayWriter never sees this frame.
    skipFrame = dropNextFrame;

    int frameDelayMs = 0;
    playing = gif.playFrame(false, &frameDelayMs);
    nextFrameUs += (uint64_t)frameDelayMs * 1000ULL;

    prevFrameWasOpaque = !currentFrameHasTransparency;

    if (!skipFrame) {
      lastPosted = decodeBuffer;
    }

    // After every frame, compare heard audio position to the GIF timeline.
    // If audio has overtaken the end of this frame, mark the next frame for
    // dropping. This self-corrects any drift or decode-time spikes without
    // touching audio at all.
    dropNextFrame = false;
    if (haveAudio && frameDelayMs > 0 && player.isActive()) {
      int64_t audioUs = player.getAudioPositionUs();
      int64_t videoUs = (int64_t)(nextFrameUs - startUs);  // GIF timeline pos of next frame start
      if (audioUs >= 0) {
        dropNextFrame = (audioUs > videoUs);
      }
    }

    decodeBuffer ^= 1;
  }

  if (lastPosted != -1) {
    waitForLastFrame(lastPosted);
  }

  stopDisplayWriterTask();
  gif.close();
  return true;
}

void *GifRenderer::openFile(const char *fname, int32_t *pSize) {
  gifFile = SD_MMC.open(fname, FILE_READ);
  if (gifFile) {
    *pSize = gifFile.size();
    return (void *)&gifFile;
  }
  return NULL;
}

void GifRenderer::closeFile(void *pHandle) {
}

int32_t GifRenderer::readFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen) {
  int32_t iBytesRead = iLen;
  File *f = static_cast<File *>(pFile->fHandle);

  if ((pFile->iSize - pFile->iPos) < iLen) {
    iBytesRead = pFile->iSize - pFile->iPos;
  }
  if (iBytesRead <= 0) {
    return 0;
  }

  iBytesRead = (int32_t)f->read(pBuf, iBytesRead);
  pFile->iPos += iBytesRead;
  return iBytesRead;
}

int32_t GifRenderer::seekFile(GIFFILE *pFile, int32_t iPosition) {
  File *f = static_cast<File *>(pFile->fHandle);
  f->seek(iPosition);
  pFile->iPos = iPosition;
  return iPosition;
}

bool GifRenderer::clipAndPrepareLine(
  GIFDRAW *pDraw,
  int &iWidth,
  int &y,
  uint8_t *&pixels,
  uint16_t *&lineBase
) {
  if (tft == NULL) {
    return false;
  }

  iWidth = min(pDraw->iWidth, DISPLAY_WIDTH - pDraw->iX);
  if (pDraw->iX >= DISPLAY_WIDTH || iWidth < 1) {
    return false;
  }

  y = pDraw->iY + pDraw->y;
  if (y >= DISPLAY_HEIGHT) {
    return false;
  }

  pixels = pDraw->pPixels;
  lineBase = &frameBuffer[decodeBuffer][y * DISPLAY_WIDTH + pDraw->iX];
  return true;
}

void GifRenderer::applyDisposalIfNeeded(GIFDRAW *pDraw, uint8_t *pixels, int iWidth) {
  if (pDraw->ucDisposalMethod != 2) {
    return;
  }

  int x = 0;
  while (x < iWidth) {
    if (pixels[x] == pDraw->ucTransparent) {
      pixels[x] = pDraw->ucBackground;
    }
    x++;
  }

  pDraw->ucHasTransparency = 0;
}

void GifRenderer::drawTransparentLine(
  GIFDRAW *pDraw,
  uint8_t *pixels,
  int iWidth,
  uint16_t *lineBase,
  uint16_t *palette
) {
  uint8_t *src = pixels;
  uint8_t *srcEnd = src + iWidth;
  uint8_t transparentIndex = pDraw->ucTransparent;
  int x = 0;

  while (src < srcEnd) {
    uint8_t colorIndex = *src++;
    if (colorIndex != transparentIndex) {
      lineBase[x] = palette[colorIndex];
    }
    x++;
  }
}

void GifRenderer::drawOpaqueLine(
  uint8_t *pixels,
  int iWidth,
  uint16_t *lineBase,
  uint16_t *palette
) {
  const uint8_t *src = pixels;
  const uint8_t *end = pixels + iWidth;
  uint16_t      *dst = lineBase;

  while (src < end) {
    *dst++ = palette[*src++];
  }
}

void GifRenderer::flushFrameIfLastLine(GIFDRAW *pDraw) {
  if (pDraw->y != pDraw->iHeight - 1) {
    return;
  }
  if (skipFrame) {
    // Don't display this frame; release the buffer so the next frame can use it.
    xSemaphoreGive(bufferFree[decodeBuffer]);
  } else {
    xQueueSend(displayQueue, &decodeBuffer, portMAX_DELAY);
  }
}

void GifRenderer::draw(GIFDRAW *pDraw) {
  int iWidth;
  int y;
  uint8_t  *pixels;
  uint16_t *lineBase;

  if (!clipAndPrepareLine(pDraw, iWidth, y, pixels, lineBase)) {
    return;
  }

  uint16_t *palette = pDraw->pPalette;

  applyDisposalIfNeeded(pDraw, pixels, iWidth);

  if (pDraw->ucHasTransparency) {
    currentFrameHasTransparency = true;
    drawTransparentLine(pDraw, pixels, iWidth, lineBase, palette);
  } else {
    drawOpaqueLine(pixels, iWidth, lineBase, palette);
  }

  flushFrameIfLastLine(pDraw);
}

void GifRenderer::clearFrameBuffer() {
  memset(frameBuffer, 0, sizeof(frameBuffer));
}

void GifRenderer::displayWriterTask() {
  int bufIdx;
  while (true) {
    if (xQueueReceive(displayQueue, &bufIdx, portMAX_DELAY) != pdTRUE) {
      vTaskDelete(NULL);
      return;
    }
    if (bufIdx == -1) {
      vTaskDelete(NULL);
      return;
    }
    tft->startWrite();
    tft->setAddrWindow(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    tft->writePixels(frameBuffer[bufIdx], FRAMEBUFFER_SIZE, false, true);
    tft->endWrite();
    xSemaphoreGive(bufferFree[bufIdx]);
  }
}

void GifRenderer::displayWriterTaskEntry(void *param) {
  static_cast<GifRenderer *>(param)->displayWriterTask();
}

void GifRenderer::startDisplayWriterTask() {
  decodeBuffer = 0;
  xTaskCreatePinnedToCore(
    displayWriterTaskEntry, "DisplayWriter",
    4096, this,
    2,
    &displayWriterHandle,
    1
  );
}

void GifRenderer::stopDisplayWriterTask() {
  if (displayWriterHandle == NULL) return;
  int sentinel = -1;
  xQueueSend(displayQueue, &sentinel, portMAX_DELAY);
  displayWriterHandle = NULL;
}

void GifRenderer::waitForLastFrame(int lastPosted) {
  xSemaphoreTake(bufferFree[lastPosted], portMAX_DELAY);
  xSemaphoreGive(bufferFree[lastPosted]);
}

// Free-function callbacks

void *GIFOpenFile(const char *fname, int32_t *pSize) {
  return GifRenderer::instance().openFile(fname, pSize);
}

void GIFCloseFile(void *pHandle) {
  GifRenderer::instance().closeFile(pHandle);
}

int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen) {
  return GifRenderer::instance().readFile(pFile, pBuf, iLen);
}

int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition) {
  return GifRenderer::instance().seekFile(pFile, iPosition);
}

void GIFDraw(GIFDRAW *pDraw) {
  GifRenderer::instance().draw(pDraw);
}

