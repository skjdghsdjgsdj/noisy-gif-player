#include "Application.h"

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "SDCard.h"
#include "PreferenceManager.h"
#include "GifSelector.h"
#include "GifRenderer.h"
#include "I2SWavPlayer.h"
#include "USBMassStorageMode.h"

// Pins
static constexpr uint8_t USB_MSC_BUTTON_PIN = 2;            // Front-panel button used only to enter USB MSC mode
static constexpr uint8_t BACKLIGHT_PIN     = TFT_BACKLITE;  // TFT backlight control pin
static constexpr uint8_t TFT_I2C_POWER_PIN = TFT_I2C_POWER; // TFT I2C power control pin

// Backlight / fade constants
static constexpr uint32_t FADE_DURATION_MS = 500;      // Duration of backlight fade before deep sleep (ms)
static constexpr uint32_t TFT_SPI_SPEED_HZ = 80000000; // SPI clock for TFT (Hz)
static constexpr uint8_t  TFT_ROTATION     = 3;        // Display rotation setting for landscape layout

// Full-range PWM brightness; 255 is the natural max for 8-bit PWM.
static constexpr uint8_t BACKLIGHT_MAX = 255; // Full-scale backlight value for fade calculations

// Global display
static Adafruit_ST7789 tft(TFT_CS, TFT_DC, TFT_RST);

Application::Application() {
}

void Application::fadeBacklightOff() {
  uint32_t startTime = millis();
  uint32_t elapsed;

  while ((elapsed = millis() - startTime) < FADE_DURATION_MS) {
    int brightness = BACKLIGHT_MAX - (BACKLIGHT_MAX * elapsed / FADE_DURATION_MS);
    analogWrite(BACKLIGHT_PIN, brightness);
    delay(1);
  }

  digitalWrite(BACKLIGHT_PIN, LOW);
}

void Application::powerDownPeripherals() {
  pinMode(NEOPIXEL_POWER, OUTPUT);
  digitalWrite(NEOPIXEL_POWER, LOW);
  digitalWrite(TFT_I2C_POWER_PIN, LOW);

  SPI.end();
  SDCard::instance().end();
}

void Application::startESPDeepSleep() {
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
  esp_sleep_pd_config(ESP_PD_DOMAIN_VDDSDIO, ESP_PD_OPTION_OFF);
  esp_deep_sleep_start();
}

void Application::enterDeepSleep() {
  fadeBacklightOff();
  powerDownPeripherals();
  startESPDeepSleep();
}

void Application::setupPinsAndPower() {
  pinMode(USB_MSC_BUTTON_PIN, INPUT_PULLDOWN);

  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);

  pinMode(TFT_I2C_POWER_PIN, OUTPUT);
  digitalWrite(TFT_I2C_POWER_PIN, HIGH);
}

void Application::setupDisplay() {
  SPI.begin();
  tft.setSPISpeed(TFT_SPI_SPEED_HZ);
  tft.init(GifRenderer::DISPLAY_HEIGHT, GifRenderer::DISPLAY_WIDTH);
  tft.setRotation(TFT_ROTATION);
  tft.fillScreen(ST77XX_BLACK);
}

void Application::setupStorage() {
  SDCard::instance().begin();
}

void Application::seedRandom() {
  randomSeed(esp_random());
}

void Application::setupSubsystems() {
  GifRenderer::instance().begin(tft);
  (void)PreferenceManager::instance();
  (void)GifSelector::instance();
  (void)I2SWavPlayer::instance();
  (void)USBMassStorageMode::instance();
}

void Application::setup() {
  setupPinsAndPower();
  setupDisplay();
  setupStorage();
  seedRandom();
  setupSubsystems();

  if (digitalRead(USB_MSC_BUTTON_PIN) == HIGH) {
    USBMassStorageMode::instance().run(tft);
  }
}

void Application::loopOnce() {
  String gifPath;
  String wavPath;

  GifSelector &selector = GifSelector::instance();
  if (!selector.chooseRandomGifAndWav(gifPath, wavPath)) {
    enterDeepSleep();
  }

  PreferenceManager::instance().storeLastGifPath(gifPath);

  I2SWavPlayer &player = I2SWavPlayer::instance();
  bool haveWav = player.start(wavPath);

  if (!GifRenderer::instance().playGif(gifPath)) {
    enterDeepSleep();
  }

  if (haveWav) {
    player.waitUntilDone();
  }

  enterDeepSleep();
}

