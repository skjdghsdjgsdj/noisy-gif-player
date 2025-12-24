#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <AnimatedGIF.h>
#include <SD_MMC.h>
#include <SPI.h>
#include "driver/i2s.h"
#include "esp_sleep.h"

#define DISPLAY_WIDTH   240
#define DISPLAY_HEIGHT  135
#define FRAMEBUFFER_SIZE (DISPLAY_WIDTH * DISPLAY_HEIGHT)

// Directories
#define GIF_DIR  "/gifs"
#define WAV_DIR  "/wavs"

// I2S pins (GPIO numbers)
#define I2S_LRC   37   // WS / LRCLK
#define I2S_BCLK  38   // BCLK
#define I2S_DIN   39   // SD / DIN

// Audio buffer size - 2KB for ~64ms chunks (balanced for GIF framerate)
#define AUDIO_BUFFER_SIZE 2048

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
AnimatedGIF gif;
File gifFile;
File wavFile;

// Full-frame RGB565 backbuffer (240x135)
static uint16_t frameBuffer[FRAMEBUFFER_SIZE];

// Global audio buffer
static uint8_t audioBuffer[AUDIO_BUFFER_SIZE];

bool playedOnce = false;
bool i2sInitialized = false;
uint32_t currentSampleRate = 16000;

// Dual-core synchronization
volatile bool audioActive = false;
volatile bool playbackComplete = false;
TaskHandle_t audioTaskHandle = NULL;

// ----------- Utility: random GIF + matching WAV selection -----------

bool chooseRandomGifAndWav(String &gifPath, String &wavPath) {
  File dir = SD_MMC.open(GIF_DIR);
  if (!dir || !dir.isDirectory()) return false;

  String candidates[64];
  size_t count = 0;

  File f = dir.openNextFile();
  while (f && count < 64) {
    if (!f.isDirectory()) {
      String path = f.name();
      if (!path.startsWith(GIF_DIR)) {
        if (!path.startsWith("/")) path = String(GIF_DIR) + "/" + path;
        else path = String(GIF_DIR) + path;
      }
      int slashPos = path.lastIndexOf('/');
      String base = (slashPos >= 0) ? path.substring(slashPos + 1) : path;
      if (base.length() > 0 && base[0] != '.') {
        String lower = base;
        lower.toLowerCase();
        if (lower.endsWith(".gif")) {
          candidates[count++] = path;
        }
      }
    }
    f = dir.openNextFile();
  }
  dir.close();
  if (count == 0) return false;

  size_t idx = random(0, count);
  gifPath = candidates[idx];

  int slashPos = gifPath.lastIndexOf('/');
  String base = (slashPos >= 0) ? gifPath.substring(slashPos + 1) : gifPath;
  int dotPos = base.lastIndexOf('.');
  if (dotPos >= 0) base = base.substring(0, dotPos);
  wavPath = String(WAV_DIR) + "/" + base + ".wav";

  return true;
}

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
    iBytesRead = pFile->iSize - pFile->iPos - 1;
  if (iBytesRead <= 0) return 0;
  iBytesRead = (int32_t)f->read(pBuf, iBytesRead);
  pFile->iPos = f->position();
  return iBytesRead;
}

int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition) {
  File *f = (File *)pFile->fHandle;
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
    tft.writePixels(frameBuffer, FRAMEBUFFER_SIZE, false, false);
    tft.endWrite();
  }
}

// ---------- I2S setup and WAV helpers ----------

void setupI2S() {
  if (i2sInitialized) return;
  
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = currentSampleRate,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S_MSB,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 16,   // Changed: 16 buffers instead of 8
    .dma_buf_len = 512,    // Changed: 512 samples instead of 1024
    .use_apll = true,
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
  
  i2sInitialized = true;
}

// Parse WAV header and extract sample rate, then skip to data
bool openWav(const String &wavPath) {
  wavFile = SD_MMC.open(wavPath.c_str(), FILE_READ);
  if (!wavFile) return false;
  if (wavFile.size() < 44) return false;
  
  // Read sample rate from WAV header at offset 24 (little-endian 32-bit)
  wavFile.seek(24);
  uint8_t rateBytes[4];
  wavFile.read(rateBytes, 4);
  currentSampleRate = rateBytes[0] | (rateBytes[1] << 8) | (rateBytes[2] << 16) | (rateBytes[3] << 24);
  
  // Skip to data after 44-byte header
  wavFile.seek(44);
  return true;
}

// ---------- Audio task running on Core 0 ----------

void audioTask(void *parameter) {
  while (audioActive) {
    if (!wavFile || !wavFile.available()) {
      audioActive = false;
      break;
    }

    size_t toRead = AUDIO_BUFFER_SIZE;
    size_t remaining = wavFile.size() - wavFile.position();
    if (remaining == 0) {
      audioActive = false;
      break;
    }
    if (toRead > remaining) toRead = remaining;

    size_t n = wavFile.read(audioBuffer, toRead);
    if (n == 0) {
      audioActive = false;
      break;
    }

    size_t written = 0;
    i2s_write(I2S_NUM_0, audioBuffer, n, &written, portMAX_DELAY);
    
    vTaskDelay(1);
  }
  
  vTaskDelete(NULL);
}

// ---------- Deep sleep helper: optimized for lowest power consumption ----------

void enterDeepSleepUntilReset() {
  digitalWrite(TFT_BACKLITE, LOW);
  digitalWrite(TFT_I2C_POWER, LOW);
  
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
  esp_sleep_pd_config(ESP_PD_DOMAIN_VDDSDIO, ESP_PD_OPTION_OFF);
  
  esp_deep_sleep_start();
}

// ---------- Setup / Loop ----------

void setup() {
  pinMode(TFT_BACKLITE, OUTPUT);
  digitalWrite(TFT_BACKLITE, HIGH);

  pinMode(TFT_I2C_POWER, OUTPUT);
  digitalWrite(TFT_I2C_POWER, HIGH);
  delay(10);

  SPI.begin();
  tft.setSPISpeed(80000000);
  tft.init(DISPLAY_HEIGHT, DISPLAY_WIDTH);
  tft.setRotation(3);
  tft.fillScreen(ST77XX_BLACK);

  memset(frameBuffer, 0, sizeof(frameBuffer));

  SD_MMC.setPins(18, 16, 17, 14, 8, 15);
  SD_MMC.begin("/sdcard", true, true, 80000);

  gif.begin(LITTLE_ENDIAN_PIXELS);
  
  randomSeed(esp_random());
}

void loop() {
  if (playedOnce) {
    enterDeepSleepUntilReset();
  }

  String gifPath, wavPath;
  chooseRandomGifAndWav(gifPath, wavPath);

  bool haveWav = SD_MMC.exists(wavPath.c_str());
  if (haveWav) {
    if (!openWav(wavPath)) haveWav = false;
  }

  if (haveWav) {
    setupI2S();
  }

  if (!gif.open(gifPath.c_str(),
                GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw)) {
    if (haveWav) wavFile.close();
    playedOnce = true;
    enterDeepSleepUntilReset();
  }

  gif.playFrame(true, NULL);

  if (haveWav) {
    audioActive = true;
    xTaskCreatePinnedToCore(
      audioTask,
      "AudioTask",
      4096,
      NULL,
      1,
      &audioTaskHandle,
      0
    );
  }

  bool moreFrames = true;
  while (moreFrames) {
    moreFrames = gif.playFrame(true, NULL);
    
    if (haveWav && !audioActive) {
      break;
    }
    
    yield();
  }

  if (haveWav && audioActive) {
    audioActive = false;
    if (audioTaskHandle != NULL) {
      vTaskDelay(100 / portTICK_PERIOD_MS);
      audioTaskHandle = NULL;
    }
  }

  gif.close();
  if (haveWav) wavFile.close();

  playedOnce = true;
  enterDeepSleepUntilReset();
}
