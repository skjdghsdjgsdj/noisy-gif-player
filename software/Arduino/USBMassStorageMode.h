#pragma once
#include <Arduino.h>
#include <Adafruit_ST7789.h>
#include <USBMSC.h>

class USBMassStorageMode {
public:
  static USBMassStorageMode &instance() {
    static USBMassStorageMode inst;
    return inst;
  }

  void run(Adafruit_ST7789 &tft);

private:
  USBMassStorageMode();

  USBMSC msc;

  void showScreen(Adafruit_ST7789 &tft);
  void configureMSC();

  static int32_t onReadThunk(uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize);
  static int32_t onWriteThunk(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize);
  static bool    onStartStopThunk(uint8_t power_condition, bool start, bool load_eject);
};

