#include "USBMassStorageMode.h"
#include <USB.h>
#include "SDCard.h"
#include "GifRenderer.h"

USBMassStorageMode::USBMassStorageMode()
  : msc() {
}

void USBMassStorageMode::run(Adafruit_ST7789 &tft) {
  showScreen(tft);
  configureMSC();
  USB.begin();

  while (1) {
    delay(1000);
  }
}

void USBMassStorageMode::showScreen(Adafruit_ST7789 &tft) {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);

  // Main line: "USB connected"
  const char *mainText = "USB connected";
  tft.setTextSize(2);

  int16_t mainX, mainY;
  uint16_t mainW, mainH;
  tft.getTextBounds(mainText, 0, 0, &mainX, &mainY, &mainW, &mainH);
  int16_t mainXPos = (GifRenderer::DISPLAY_WIDTH  - (int16_t)mainW) / 2;
  int16_t mainYPos = (GifRenderer::DISPLAY_HEIGHT - (int16_t)mainH) / 2;

  tft.setCursor(mainXPos, mainYPos);
  tft.println(mainText);

  // Sub line: "Press button when done"
  const char *subText = "Press button when done";
  tft.setTextSize(1);

  int16_t subX, subY;
  uint16_t subW, subH;
  tft.getTextBounds(subText, 0, 0, &subX, &subY, &subW, &subH);
  int16_t subXPos = (GifRenderer::DISPLAY_WIDTH  - (int16_t)subW) / 2;
  // 8 pixels spacing between lines is a small visual tweak; unlikely to change.
  int16_t subYPos = mainYPos + mainH + 8;

  tft.setCursor(subXPos, subYPos);
  tft.println(subText);
}

void USBMassStorageMode::configureMSC() {
  msc.vendorID("Adafruit");
  msc.productID("ESP32-S3 SD");
  msc.productRevision("1.0");

  msc.onRead(onReadThunk);
  msc.onWrite(onWriteThunk);
  msc.onStartStop(onStartStopThunk);

  msc.mediaPresent(true);
  msc.begin(SDCard::instance().numSectors(), SDCard::instance().sectorSize());
}

int32_t USBMassStorageMode::onReadThunk(uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
  (void)offset;
  return SDCard::instance().rawRead(lba, buffer, bufsize);
}

int32_t USBMassStorageMode::onWriteThunk(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize) {
  (void)offset;
  return SDCard::instance().rawWrite(lba, buffer, bufsize);
}

bool USBMassStorageMode::onStartStopThunk(uint8_t power_condition, bool start, bool load_eject) {
  (void)power_condition;
  (void)start;
  (void)load_eject;
  return true;
}

