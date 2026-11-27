#include "howl_player.h"

#ifndef WOKWI_SIM

#include <Arduino.h>

#include "wolf_howl.h"
#include <driver/sigmadelta.h>
#include <esp_timer.h>

namespace {

constexpr int kHowlPin = 18;
// The strong part of the howl is deviation 59. Duty stays near ±10 so volume
// 15 remains on the unclipped side of the amplifier.
constexpr int kHowlSampleLoud = 59;
constexpr int kHowlDutyPeak = 10;
// 80 MHz / 10 = 8 MHz. Fast enough for the 22 nF to average the pulses.
constexpr uint8_t kHowlPrescale = 10;
constexpr uint8_t kHowlVolumeMax = 15;
constexpr uint64_t kHowlPeriodUs = 1000000ULL / kWolfHowlRate;

volatile size_t howlIndex = 0;
volatile bool howlActive = false;
volatile uint8_t howlVolume = kHowlVolumeMax;
int howlError = 0;
esp_timer_handle_t howlTimer = nullptr;
bool howlReady = false;

void releaseHowlPin() {
  howlActive = false;
  howlError = 0;
  if (howlTimer != nullptr) {
    esp_timer_stop(howlTimer);
  }
  if (!howlReady) {
    return;
  }
  // Duty 0 holds the pin at half of 3.3 V. Let it go so that voltage does not
  // sit on the amplifier between howls.
  sigmadelta_set_duty(SIGMADELTA_CHANNEL_0, 0);
  sigmaDeltaDetachPin(kHowlPin);
  pinMode(kHowlPin, INPUT);
}

void howlTick(void *) {
  if (!howlActive) {
    return;
  }
  if (howlIndex >= kWolfHowlSamples) {
    releaseHowlPin();
    return;
  }
  const int dev = static_cast<int>(kWolfHowl[howlIndex++]) - 128;
  const int denom = kHowlSampleLoud * kHowlVolumeMax;
  const int scaled = dev * kHowlDutyPeak * static_cast<int>(howlVolume) + howlError;
  int sample = scaled / denom;
  howlError = scaled - sample * denom;
  if (sample > kHowlDutyPeak) {
    sample = kHowlDutyPeak;
  } else if (sample < -kHowlDutyPeak) {
    sample = -kHowlDutyPeak;
  }
  sigmadelta_set_duty(SIGMADELTA_CHANNEL_0, static_cast<int8_t>(sample));
}

void ensureHowl() {
  if (howlReady) {
    return;
  }
  sigmadelta_config_t config = {};
  config.channel = SIGMADELTA_CHANNEL_0;
  config.sigmadelta_gpio = static_cast<gpio_num_t>(kHowlPin);
  config.sigmadelta_prescale = kHowlPrescale;
  config.sigmadelta_duty = 0;
  if (sigmadelta_config(&config) != ESP_OK) {
    return;
  }

  esp_timer_create_args_t args = {};
  args.callback = &howlTick;
  args.name = "howl";
  if (esp_timer_create(&args, &howlTimer) == ESP_OK) {
    howlReady = true;
  }
}

} // namespace

void howlSetVolume(uint8_t volume) {
  if (volume > kHowlVolumeMax) {
    volume = kHowlVolumeMax;
  }
  howlVolume = volume;
}

void howlPlay() {
  ensureHowl();
  if (howlTimer == nullptr) {
    return;
  }
  howlActive = false;
  esp_timer_stop(howlTimer);
  howlIndex = 0;
  howlError = 0;
  sigmadelta_set_pin(SIGMADELTA_CHANNEL_0, static_cast<gpio_num_t>(kHowlPin));
  sigmadelta_set_duty(SIGMADELTA_CHANNEL_0, 0);
  howlActive = true;
  esp_timer_start_periodic(howlTimer, kHowlPeriodUs);
}

void howlStop() {
  releaseHowlPin();
}

bool howlPlaying() { return howlActive; }

#else

void howlSetVolume(uint8_t) {}

void howlPlay() {}

void howlStop() {}

bool howlPlaying() { return false; }

#endif
