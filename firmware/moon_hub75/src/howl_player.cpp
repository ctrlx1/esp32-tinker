#include "howl_player.h"

#ifndef WOKWI_SIM

#include <Arduino.h>

#include "wolf_howl.h"
#include <driver/sigmadelta.h>
#include <esp_timer.h>

namespace {

constexpr int kHowlPin = 18;
constexpr uint64_t kHowlPeriodUs = 1000000ULL / kWolfHowlRate;

volatile size_t howlIndex = 0;
volatile bool howlActive = false;
esp_timer_handle_t howlTimer = nullptr;
bool howlReady = false;

void howlTick(void *) {
  if (!howlActive) {
    return;
  }
  if (howlIndex >= kWolfHowlSamples) {
    sigmadelta_set_duty(SIGMADELTA_CHANNEL_0, 0);
    howlActive = false;
    return;
  }
  const int sample = (static_cast<int>(kWolfHowl[howlIndex++]) - 128) * 90 / 128;
  sigmadelta_set_duty(SIGMADELTA_CHANNEL_0, static_cast<int8_t>(sample));
}

void ensureHowl() {
  if (howlReady) {
    return;
  }
  sigmadelta_config_t config = {};
  config.channel = SIGMADELTA_CHANNEL_0;
  config.sigmadelta_gpio = kHowlPin;
  config.sigmadelta_prescale = 0;
  config.sigmadelta_duty = 0;
  sigmadelta_config(&config);

  esp_timer_create_args_t args = {};
  args.callback = &howlTick;
  args.name = "howl";
  if (esp_timer_create(&args, &howlTimer) == ESP_OK) {
    howlReady = true;
  }
}

} // namespace

void howlPlay() {
  ensureHowl();
  if (howlTimer == nullptr) {
    return;
  }
  howlActive = false;
  esp_timer_stop(howlTimer);
  howlIndex = 0;
  howlActive = true;
  esp_timer_start_periodic(howlTimer, kHowlPeriodUs);
}

void howlStop() {
  howlActive = false;
  if (howlTimer != nullptr) {
    esp_timer_stop(howlTimer);
  }
  if (howlReady) {
    sigmadelta_set_duty(SIGMADELTA_CHANNEL_0, 0);
  }
}

bool howlPlaying() { return howlActive; }

#else

void howlPlay() {}

void howlStop() {}

bool howlPlaying() { return false; }

#endif
