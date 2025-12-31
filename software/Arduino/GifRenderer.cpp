#include "GifRenderer.h"
#include <SD_MMC.h>

GifRenderer::GifRenderer()
  : tft(NULL) {
}

GifRenderer &GifRenderer::instance() {
  static GifRenderer inst;
  return inst;
}

void GifRenderer::begin(Adafruit_ST7789 &display) {
  tft = &display;
  gif.begin(LITTLE_ENDIAN_PIXELS);
  clearFrameBuffer();
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

  bool playing = true;
  while (playing) {
    playing = gif.playFrame(true, NULL);
  }

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
    iBytesRead = pFile->iSize - pFile->iPos - 1;
  }
  if (iBytesRead <= 0) {
    return 0;
  }

  iBytesRead = (int32_t)f->read(pBuf, iBytesRead);
  pFile->iPos = f->position();
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
  lineBase = &frameBuffer[y * DISPLAY_WIDTH + pDraw->iX];
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
  uint16_t pixelCache[DISPLAY_WIDTH];

  int x = 0;
  int runLength = 0;

  while (x < iWidth) {
    uint8_t colorIndex = transparentIndex - 1;
    uint16_t *cachePtr = pixelCache;

    while (colorIndex != transparentIndex && src < srcEnd) {
      colorIndex = *src++;
      if (colorIndex == transparentIndex) {
        src--;
      } else {
        *cachePtr = palette[colorIndex];
        cachePtr++;
        runLength++;
      }
    }

    if (runLength > 0) {
      int i = 0;
      while (i < runLength) {
        lineBase[x + i] = pixelCache[i];
        i++;
      }
      x += runLength;
      runLength = 0;
    }

    colorIndex = transparentIndex;
    while (colorIndex == transparentIndex && src < srcEnd) {
      colorIndex = *src++;
      if (colorIndex == transparentIndex) {
        x++;
      } else {
        src--;
      }
    }
  }
}

void GifRenderer::drawOpaqueLine(
  uint8_t *pixels,
  int iWidth,
  uint16_t *lineBase,
  uint16_t *palette
) {
  uint8_t  *src = pixels;
  uint16_t *dst = lineBase;
  int x = 0;

  while (x < iWidth) {
    *dst = palette[*src];
    dst++;
    src++;
    x++;
  }
}

void GifRenderer::flushFrameIfLastLine(GIFDRAW *pDraw) {
  if (pDraw->y != pDraw->iHeight - 1) {
    return;
  }

  tft->startWrite();
  tft->setAddrWindow(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
  tft->writePixels(frameBuffer, FRAMEBUFFER_SIZE, false, false);
  tft->endWrite();
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
    drawTransparentLine(pDraw, pixels, iWidth, lineBase, palette);
  } else {
    drawOpaqueLine(pixels, iWidth, lineBase, palette);
  }

  flushFrameIfLastLine(pDraw);
}

void GifRenderer::clearFrameBuffer() {
  size_t i = 0;
  while (i < FRAMEBUFFER_SIZE) {
    frameBuffer[i] = 0;
    i++;
  }
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

