#include "howl_player.h"

#ifndef WOKWI_SIM

#include <Arduino.h>

#include "wolf_howl.h"
#include <esp_timer.h>

namespace {

constexpr int kHowlPin = 18;
constexpr uint8_t kHowlChannel = 0;
// A steady 150 kHz pulse. The 22 nF filter averages it. The old sigma-delta
// stream was noisy enough to crackle through that same filter.
constexpr uint32_t kHowlPwmHz = 150000;
constexpr uint8_t kHowlPwmBits = 8;
constexpr int kHowlPwmMid = 128;
// The strong part of the howl is deviation 59. Volume 15 stops near the
// highest setting that stayed unclipped on the previous scale.
constexpr int kHowlSampleLoud = 59;
constexpr int kHowlDutyPeak = 8;
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
  // Mid duty holds the pin at half of 3.3 V. Let it go so that voltage does
  // not sit on the amplifier between howls.
  ledcWrite(kHowlChannel, kHowlPwmMid);
  ledcDetachPin(kHowlPin);
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
    howlError = 0;
  } else if (sample < -kHowlDutyPeak) {
    sample = -kHowlDutyPeak;
    howlError = 0;
  }
  ledcWrite(kHowlChannel, kHowlPwmMid + sample);
}

void ensureHowl() {
  if (howlReady) {
    return;
  }
  if (ledcSetup(kHowlChannel, kHowlPwmHz, kHowlPwmBits) == 0) {
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
  ledcAttachPin(kHowlPin, kHowlChannel);
  ledcWrite(kHowlChannel, kHowlPwmMid);
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
