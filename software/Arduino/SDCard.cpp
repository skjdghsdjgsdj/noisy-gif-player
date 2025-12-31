#include "SDCard.h"

// SD card constants
// Base SD card clock and mountpoint; users might tweak frequency or mount path.
static constexpr const char* SD_MOUNT_POINT  = "/sdcard"; // Mount path for SD_MMC
static constexpr uint32_t    SD_FREQ_KHZ     = 80000;     // SD bus frequency in kHz

// SDCard CMD/CLK pins for ESP32-S3 Reverse TFT Feather; unlikely to change unless board changes.
static constexpr int SD_CMD_PIN = 18; // SDMMC CMD pin
static constexpr int SD_CLK_PIN = 16; // SDMMC CLK pin

// SDCard 4-bit data pins grouped together; board-specific wiring.
static constexpr int SD_DATA_PINS[4] = {
  17, // SDMMC D0 pin
  14, // SDMMC D1 pin
  8,  // SDMMC D2 pin
  15  // SDMMC D3 pin
};

SDCard::SDCard() {
}

void SDCard::begin() {
  // Initialize SD_MMC with explicit command/clock/data pins for this board.
  SD_MMC.setPins(
    SD_CMD_PIN,
    SD_CLK_PIN,
    SD_DATA_PINS[0],
    SD_DATA_PINS[1],
    SD_DATA_PINS[2],
    SD_DATA_PINS[3]
  );

  SD_MMC.begin(SD_MOUNT_POINT, true, true, SD_FREQ_KHZ);
}

void SDCard::end() {
  SD_MMC.end();
}

File SDCard::open(const String &path, const char *mode) {
  bool isRead  = (mode[0] == 'r');
  bool isWrite = (mode[0] == 'w');

  return isRead
    ? SD_MMC.open(path.c_str(), FILE_READ)
    : (isWrite
        ? SD_MMC.open(path.c_str(), FILE_WRITE)
        : SD_MMC.open(path.c_str()));
}

int32_t SDCard::rawRead(uint32_t lba, void *buffer, uint32_t bufsize) {
  return rawTransfer(lba, buffer, bufsize, true);
}

int32_t SDCard::rawWrite(uint32_t lba, uint8_t *buffer, uint32_t bufsize) {
  return rawTransfer(lba, buffer, bufsize, false);
}

int32_t SDCard::rawTransfer(uint32_t lba, void *buffer, uint32_t bufsize, bool isRead) {
  uint8_t* buf = (uint8_t*)buffer;
  uint32_t blocks = bufsize / 512; // SD sectors are 512 bytes

  uint32_t blockIndex = 0;
  while (blockIndex < blocks) {
    uint8_t *blockPtr = buf + (blockIndex * 512); // Pointer to current sector buffer

    bool ok = isRead
      ? SD_MMC.readRAW(blockPtr, lba + blockIndex)
      : SD_MMC.writeRAW(blockPtr, lba + blockIndex);

    if (!ok) {
      return -1;
    }

    blockIndex++;
  }

  return (int32_t)bufsize;
}

uint32_t SDCard::numSectors() {
  return SD_MMC.numSectors();
}

uint32_t SDCard::sectorSize() {
  return SD_MMC.sectorSize();
}

