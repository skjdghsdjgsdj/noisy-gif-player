#pragma once
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <AnimatedGIF.h>
#include <FS.h>

class GifRenderer {
public:
  static GifRenderer &instance();

  void begin(Adafruit_ST7789 &display);
  bool playGif(const String &gifPath);

  void *openFile(const char *fname, int32_t *pSize);
  void  closeFile(void *pHandle);
  int32_t readFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen);
  int32_t seekFile(GIFFILE *pFile, int32_t iPosition);
  void  draw(GIFDRAW *pDraw);

  // Authoritative display size for the project; used by other modules for layout.
  static constexpr int DISPLAY_WIDTH  = 240;
  static constexpr int DISPLAY_HEIGHT = 135;

private:
  GifRenderer();

  static constexpr int FRAMEBUFFER_SIZE = DISPLAY_WIDTH * DISPLAY_HEIGHT;

  Adafruit_ST7789 *tft;
  AnimatedGIF gif;
  File gifFile;
  uint16_t frameBuffer[FRAMEBUFFER_SIZE];

  void clearFrameBuffer();

  bool clipAndPrepareLine(GIFDRAW *pDraw, int &iWidth, int &y, uint8_t *&pixels, uint16_t *&lineBase);
  void applyDisposalIfNeeded(GIFDRAW *pDraw, uint8_t *pixels, int iWidth);
  void drawTransparentLine(GIFDRAW *pDraw, uint8_t *pixels, int iWidth, uint16_t *lineBase, uint16_t *palette);
  void drawOpaqueLine(uint8_t *pixels, int iWidth, uint16_t *lineBase, uint16_t *palette);
  void flushFrameIfLastLine(GIFDRAW *pDraw);
};

// Free-function callbacks for AnimatedGIF
void *GIFOpenFile(const char *fname, int32_t *pSize);
void GIFCloseFile(void *pHandle);
int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen);
int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition);
void GIFDraw(GIFDRAW *pDraw);

