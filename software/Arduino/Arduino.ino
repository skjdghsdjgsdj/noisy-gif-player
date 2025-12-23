#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <AnimatedGIF.h>
#include <SD_MMC.h>
#include <SPI.h>
#include "driver/i2s.h"

#define DISPLAY_WIDTH   240
#define DISPLAY_HEIGHT  135
#define GIF_PATH "/gifs/PXL_20251121_003548683.TS.gif"
#define WAV_PATH "/wavs/PXL_20251121_003548683.TS.wav"

// I2S pins (GPIO numbers)
#define I2S_LRC   39   // WS / LRCLK
#define I2S_BCLK  38   // BCLK
#define I2S_DIN   37   // SD / DIN

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
AnimatedGIF gif;
File gifFile;
File wavFile;

// Full-frame RGB565 backbuffer (240x135)
static uint16_t frameBuffer[DISPLAY_WIDTH * DISPLAY_HEIGHT];

// ---------- AnimatedGIF file callbacks (SD_MMC) ----------

void *GIFOpenFile(const char *fname, int32_t *pSize) {
  gifFile = SD_MMC.open(fname, FILE_READ);
  if (gifFile) {
    *pSize = gifFile.size();
    return (void *)&gifFile;
  }
  return NULL;
}

void GIFCloseFile(void *pHandle) {
  File *f = static_cast<File *>(pHandle);
  if (f != NULL) f->close();
}

int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen) {
  int32_t iBytesRead = iLen;
  File *f = static_cast<File *>(pFile->fHandle);
  if ((pFile->iSize - pFile->iPos) < iLen)
    iBytesRead = pFile->iSize - pFile->iPos - 1; // Adafruit workaround [web:61]
  if (iBytesRead <= 0) return 0;
  iBytesRead = (int32_t)f->read(pBuf, iBytesRead);
  pFile->iPos = f->position();
  return iBytesRead;
}

int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition) {
  File *f = static_cast<File *>(pFile->fHandle);
  f->seek(iPosition);
  pFile->iPos = (int32_t)f->position();
  return pFile->iPos;
}

// ---------- GIFDraw: Adafruit logic into frameBuffer, full-frame blit at end ----------

void GIFDraw(GIFDRAW *pDraw) {
  uint8_t *s;
  uint16_t *d, *usPalette, usTemp[DISPLAY_WIDTH];
  int x, y, iWidth;

  iWidth = pDraw->iWidth;
  if (iWidth + pDraw->iX > DISPLAY_WIDTH)
    iWidth = DISPLAY_WIDTH - pDraw->iX;

  usPalette = pDraw->pPalette;
  y = pDraw->iY + pDraw->y;

  if (y >= DISPLAY_HEIGHT || pDraw->iX >= DISPLAY_WIDTH || iWidth < 1)
    return;

  s = pDraw->pPixels;
  uint16_t *lineBase = &frameBuffer[y * DISPLAY_WIDTH + pDraw->iX];

  if (pDraw->ucDisposalMethod == 2) {
    for (x = 0; x < iWidth; x++) {
      if (s[x] == pDraw->ucTransparent)
        s[x] = pDraw->ucBackground;
    }
    pDraw->ucHasTransparency = 0;
  }

  if (pDraw->ucHasTransparency) {
    uint8_t *pEnd, c, ucTransparent = pDraw->ucTransparent;
    int iCount;
    pEnd = s + iWidth;
    x = 0;
    iCount = 0;

    while (x < iWidth) {
      c = ucTransparent - 1;
      d = usTemp;
      while (c != ucTransparent && s < pEnd) {
        c = *s++;
        if (c == ucTransparent) {
          s--;
        } else {
          *d++ = usPalette[c];
          iCount++;
        }
      }
      if (iCount) {
        for (int i = 0; i < iCount; i++) {
          lineBase[x + i] = usTemp[i];
        }
        x += iCount;
        iCount = 0;
      }
      c = ucTransparent;
      while (c == ucTransparent && s < pEnd) {
        c = *s++;
        if (c == ucTransparent) {
          x++;
        } else {
          s--;
        }
      }
    }
  } else {
    s = pDraw->pPixels;
    d = lineBase;
    for (x = 0; x < iWidth; x++) {
      *d++ = usPalette[*s++];
    }
  }

  if (pDraw->y == pDraw->iHeight - 1) {
    tft.startWrite();
    tft.setAddrWindow(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    tft.writePixels(frameBuffer, DISPLAY_WIDTH * DISPLAY_HEIGHT, false, false);
    tft.endWrite();
  }
}

// ---------- I2S setup and WAV helpers (1024 DMA, 2048-byte chunks) ----------

void setupI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 16000,                           // 16 kHz mono [web:117][web:128]
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S_MSB,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 1024,                            // reverted to 1024 [web:133]
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num   = I2S_BCLK,
    .ws_io_num    = I2S_LRC,
    .data_out_num = I2S_DIN,
    .data_in_num  = I2S_PIN_NO_CHANGE
  };

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
  i2s_zero_dma_buffer(I2S_NUM_0);
}

// Skip 44-byte WAV header – assumes 16-bit PCM mono 16 kHz.
bool openWav() {
  wavFile = SD_MMC.open(WAV_PATH, FILE_READ);
  if (!wavFile) return false;
  const int headerSize = 44;
  if (wavFile.size() <= headerSize) return false;
  wavFile.seek(headerSize);
  return true;
}

// ~64 ms of audio per chunk (2048 bytes @ 16 kHz 16-bit mono). [web:117]
bool pumpWavChunkBlocking() {
  static uint8_t buffer[2048];
  if (!wavFile || !wavFile.available()) return false;

  size_t toRead = sizeof(buffer);
  size_t remaining = wavFile.size() - wavFile.position();
  if (remaining == 0) return false;
  if (toRead > remaining) toRead = remaining;

  size_t n = wavFile.read(buffer, toRead);
  if (n == 0) return false;

  size_t written = 0;
  i2s_write(I2S_NUM_0, buffer, n, &written, portMAX_DELAY);
  return true;
}

// ---------- Setup / Loop ----------

void setup() {
  pinMode(TFT_BACKLITE, OUTPUT);
  digitalWrite(TFT_BACKLITE, HIGH);

  pinMode(TFT_I2C_POWER, OUTPUT);
  digitalWrite(TFT_I2C_POWER, HIGH);
  delay(10);

  SPI.begin();                                  // MISO unused, GPIO37 free for I2S [web:120]
  tft.setSPISpeed(80000000);                    // 80 MHz SPI [web:68]
  tft.init(DISPLAY_HEIGHT, DISPLAY_WIDTH);
  tft.setRotation(3);
  tft.fillScreen(ST77XX_BLACK);

  for (int i = 0; i < DISPLAY_WIDTH * DISPLAY_HEIGHT; i++) {
    frameBuffer[i] = 0;
  }

  SD_MMC.setPins(18, 16, 17, 14, 8, 15);
  SD_MMC.begin("/sdcard", true, true, 40000);   // 40 MHz SDMMC [web:82]

  setupI2S();

  gif.begin(LITTLE_ENDIAN_PIXELS);              // Adafruit pattern [web:61]
}

void loop() {
  if (!openWav()) {
    if (gif.open(GIF_PATH,
                 GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw)) {
      while (gif.playFrame(true, NULL)) { yield(); }
      gif.close();
    }
    return;
  }

  if (!gif.open(GIF_PATH,
                GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw)) {
    wavFile.close();
    return;
  }

  bool audioOK = true;
  bool moreFrames = true;

  while (audioOK && moreFrames) {
    // GIF frame with proper delay (~66 ms @ 15 fps). [web:67]
    moreFrames = gif.playFrame(true, NULL);

    // ~64 ms of audio per chunk; matches frame timing closely. [web:117]
    audioOK = pumpWavChunkBlocking();

    if (!audioOK || !moreFrames) break;

    yield();
  }

  gif.close();
  wavFile.close();

  delay(250);
}
