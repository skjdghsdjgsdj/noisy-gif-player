#pragma once
#include <Arduino.h>
#include <SD_MMC.h>

class SDCard {
public:
  static SDCard &instance() {
    static SDCard inst;
    return inst;
  }

  void begin();
  void end();

  File open(const String &path, const char *mode);
  int32_t rawRead(uint32_t lba, void *buffer, uint32_t bufsize);
  int32_t rawWrite(uint32_t lba, uint8_t *buffer, uint32_t bufsize);

  uint32_t numSectors();
  uint32_t sectorSize();

private:
  SDCard();
  int32_t rawTransfer(uint32_t lba, void *buffer, uint32_t bufsize, bool isRead);
};

