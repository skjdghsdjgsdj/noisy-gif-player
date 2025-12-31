#pragma once
#include <Arduino.h>
#include <FS.h>

class I2SWavPlayer {
public:
  static I2SWavPlayer &instance() {
    static I2SWavPlayer inst;
    return inst;
  }

  bool start(const String &wavPath);
  void waitUntilDone();

private:
  I2SWavPlayer();

  static constexpr uint32_t AUDIO_BUFFER_SIZE = 2048;
  static constexpr int I2S_LRC  = 37;
  static constexpr int I2S_BCLK = 38;
  static constexpr int I2S_DIN  = 39;

  File           wavFile;
  volatile bool  audioActive;
  uint32_t       sampleRate;
  uint8_t        audioBuffer[AUDIO_BUFFER_SIZE];

  static void audioTaskEntry(void *parameter);
  void audioTask();
  void setupI2S(uint32_t rate);

  uint32_t openWav(const String &wavPath);
  bool     openAndValidateHeader(const String &wavPath);
  uint32_t readSampleRate();
  bool     skipToDataChunk();
};

