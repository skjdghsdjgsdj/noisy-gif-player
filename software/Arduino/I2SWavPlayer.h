#pragma once
#include <Arduino.h>
#include <FS.h>
#include <atomic>

class I2SWavPlayer {
public:
  static I2SWavPlayer &instance() {
    static I2SWavPlayer inst;
    return inst;
  }

  bool start(const String &wavPath);
  void waitUntilDone();

  // True while the AudioTask is still streaming samples.
  bool isActive() const { return audioActive; }

  // Real-clock µs at which the first audio sample of the content will be heard
  // (accounts for I2S DMA pipeline latency). Valid after start() returns.
  uint64_t getAudioRenderStartUs() const { return audioRenderStartUs; }

  // µs from content start of the audio sample currently being heard.
  // Returns -1 if audio is not running.
  int64_t getAudioPositionUs() const;

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
  // Total bytes handed to i2s_write. Written only by AudioTask; read by GifRenderer.
  std::atomic<uint32_t> totalBytesWritten;
  // Real-clock µs when the first audio sample of the content will be heard.
  uint64_t audioRenderStartUs;

  static void audioTaskEntry(void *parameter);
  void audioTask();
  void setupI2S(uint32_t rate);

  uint32_t openWav(const String &wavPath);
  bool     openAndValidateHeader(const String &wavPath);
  uint32_t readSampleRate();
  bool     skipToDataChunk();
};

