#include "Application.h"

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "driver/ledc.h"
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

// Full-range PWM brightness; 255 is the natural max for 8-bit resolution.
static constexpr uint32_t BACKLIGHT_MAX = 255; // Full-scale duty cycle for LEDC 8-bit timer

// LEDC peripheral config for the backlight pin.
// Uses a dedicated channel and timer so no other subsystem can claim them.
static constexpr ledc_channel_t BACKLIGHT_LEDC_CHANNEL = LEDC_CHANNEL_0;
static constexpr ledc_timer_t   BACKLIGHT_LEDC_TIMER   = LEDC_TIMER_0;

// Global display
static Adafruit_ST7789 tft(TFT_CS, TFT_DC, TFT_RST);

Application::Application() {
}

void Application::fadeBacklightOff() {
  // Hardware LEDC fade: the peripheral drives the PWM duty from current level
  // to 0 over FADE_DURATION_MS with no CPU polling. LEDC_FADE_WAIT_DONE blocks
  // until the fade completes before we proceed to power down peripherals.
  ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, BACKLIGHT_LEDC_CHANNEL, 0, FADE_DURATION_MS);
  ledc_fade_start(LEDC_LOW_SPEED_MODE, BACKLIGHT_LEDC_CHANNEL, LEDC_FADE_WAIT_DONE);
}

void Application::powerDownPeripherals() {
  pinMode(NEOPIXEL_POWER, OUTPUT);
  digitalWrite(NEOPIXEL_POWER, LOW);
  digitalWrite(TFT_I2C_POWER_PIN, LOW);

  SPI.end();
  SDCard::instance().end();
}

[[noreturn]] void Application::startESPDeepSleep() {
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
  esp_sleep_pd_config(ESP_PD_DOMAIN_VDDSDIO, ESP_PD_OPTION_OFF);
  esp_deep_sleep_start();
}

[[noreturn]] void Application::enterDeepSleep() {
  fadeBacklightOff();
  powerDownPeripherals();
  startESPDeepSleep();
}

void Application::setupPinsAndPower() {
  pinMode(USB_MSC_BUTTON_PIN, INPUT_PULLDOWN);

  // Configure the LEDC timer and channel for hardware PWM backlight control.
  // This replaces analogWrite: LEDC lets us trigger smooth hardware fades later
  // without any CPU involvement. 8-bit resolution gives 0-255 duty range.
  ledc_timer_config_t timer_conf = {
    .speed_mode      = LEDC_LOW_SPEED_MODE,
    .duty_resolution = LEDC_TIMER_8_BIT,
    .timer_num       = BACKLIGHT_LEDC_TIMER,
    .freq_hz         = 1000,
    .clk_cfg         = LEDC_AUTO_CLK,
  };
  ledc_timer_config(&timer_conf);

  ledc_channel_config_t ch_conf = {
    .gpio_num   = BACKLIGHT_PIN,
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel    = BACKLIGHT_LEDC_CHANNEL,
    .intr_type  = LEDC_INTR_DISABLE,
    .timer_sel  = BACKLIGHT_LEDC_TIMER,
    .duty       = BACKLIGHT_MAX,  // Full brightness on boot
    .hpoint     = 0,
  };
  ledc_channel_config(&ch_conf);

  // Install the fade ISR once; required before any ledc_set_fade_with_time call.
  ledc_fade_func_install(0);

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

  I2SWavPlayer &player = I2SWavPlayer::instance();
  bool haveWav = player.start(wavPath);

  if (!GifRenderer::instance().playGif(gifPath)) {
    // The chosen path may be a stale cache entry (e.g. the file was deleted).
    // Clear the cache so the next boot re-enumerates rather than hitting the
    // same dead entry repeatedly.
    PreferenceManager::instance().clearCandidateList();
    enterDeepSleep();
  }

  if (haveWav) {
    player.waitUntilDone();
  }

  enterDeepSleep();
}

