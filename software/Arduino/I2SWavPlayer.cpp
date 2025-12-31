#include "I2SWavPlayer.h"
#include "driver/i2s.h"
#include <SD_MMC.h>

// I2S DMA configuration; users might tune these for performance.
static constexpr int I2S_DMA_BUF_COUNT = 16;  // Number of DMA buffers for I2S
static constexpr int I2S_DMA_BUF_LEN   = 512; // Samples per DMA buffer

I2SWavPlayer::I2SWavPlayer()
  : audioActive(false), sampleRate(0) {
}

bool I2SWavPlayer::start(const String &wavPath) {
  sampleRate = openWav(wavPath);
  if (sampleRate == 0) {
    return false;
  }

  setupI2S(sampleRate);
  audioActive = true;

  xTaskCreatePinnedToCore(
    audioTaskEntry,
    "AudioTask",
    4096,
    this,
    1,
    NULL,
    0
  );

  return true;
}

void I2SWavPlayer::waitUntilDone() {
  while (audioActive) {
    delay(10);
  }
  i2s_driver_uninstall(I2S_NUM_0);
}

void I2SWavPlayer::audioTaskEntry(void *parameter) {
  I2SWavPlayer *self = static_cast<I2SWavPlayer *>(parameter);
  self->audioTask();
}

void I2SWavPlayer::audioTask() {
  while (audioActive) {
    if (!wavFile || !wavFile.available()) {
      audioActive = false;
      break;
    }

    size_t remaining = wavFile.size() - wavFile.position();
    if (remaining == 0) {
      audioActive = false;
      break;
    }

    size_t toRead = min(remaining, (size_t)AUDIO_BUFFER_SIZE);

    size_t bytesRead = wavFile.read(audioBuffer, toRead);
    if (bytesRead == 0) {
      audioActive = false;
      break;
    }

    size_t bytesWritten = 0;
    i2s_write(I2S_NUM_0, audioBuffer, bytesRead, &bytesWritten, portMAX_DELAY);

    // Small yield to let other tasks run; 1 tick is sufficient and rarely changed.
    vTaskDelay(1);
  }

  vTaskDelete(NULL);
}

void I2SWavPlayer::setupI2S(uint32_t rate) {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = rate,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S_MSB,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = I2S_DMA_BUF_COUNT,
    .dma_buf_len = I2S_DMA_BUF_LEN,
    .use_apll = true,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_BCLK,
    .ws_io_num  = I2S_LRC,
    .data_out_num = I2S_DIN,
    .data_in_num  = I2S_PIN_NO_CHANGE
  };

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
}

bool I2SWavPlayer::openAndValidateHeader(const String &wavPath) {
  wavFile = SD_MMC.open(wavPath.c_str(), FILE_READ);
  if (!wavFile) {
    return false;
  }

  // 44 bytes is the canonical minimal WAV header size; users never change this.
  if (wavFile.size() < 44) {
    return false;
  }

  return true;
}

uint32_t I2SWavPlayer::readSampleRate() {
  // Sample rate at fixed offset 24 in standard PCM WAV files.
  wavFile.seek(24);
  uint8_t sampleRateBytes[4];
  wavFile.read(sampleRateBytes, 4);

  uint32_t rate =
    sampleRateBytes[0] |
    (sampleRateBytes[1] << 8) |
    (sampleRateBytes[2] << 16) |
    (sampleRateBytes[3] << 24);

  return rate;
}

bool I2SWavPlayer::skipToDataChunk() {
  // First chunk after RIFF header starts at offset 12 in standard WAV layout.
  wavFile.seek(12);
  char chunkId[4];
  uint32_t chunkSize;

  while (wavFile.available()) {
    if (wavFile.read((uint8_t*)chunkId, 4) != 4) {
      return false;
    }
    if (wavFile.read((uint8_t*)&chunkSize, 4) != 4) {
      return false;
    }

    if (memcmp(chunkId, "data", 4) == 0) {
      return true;
    }

    wavFile.seek(wavFile.position() + chunkSize);
  }

  return false;
}

uint32_t I2SWavPlayer::openWav(const String &wavPath) {
  if (!openAndValidateHeader(wavPath)) {
    return 0;
  }

  uint32_t rate = readSampleRate();
  if (rate == 0) {
    return 0;
  }

  if (!skipToDataChunk()) {
    return 0;
  }

  return rate;
}

