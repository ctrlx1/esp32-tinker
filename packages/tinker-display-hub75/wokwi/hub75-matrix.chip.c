#include "wokwi-api.h"

/* Wokwi HUB75-lite: 4 color bits per channel, LSB first. Must match
 * Hub75Display::kWokwiColorBits. */
#define HUB75_WOKWI_COLOR_BITS 4
#define HUB75_WOKWI_WIDTH 64
#define HUB75_WOKWI_HEIGHT 32

typedef struct {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t a;
} rgba_t;

typedef struct {
  pin_t r1;
  pin_t g1;
  pin_t b1;
  pin_t r2;
  pin_t g2;
  pin_t b2;
  pin_t a;
  pin_t b;
  pin_t c;
  pin_t d;
  pin_t clk;
  pin_t lat;
  buffer_t framebuffer;
  uint32_t fb_w;
  uint32_t fb_h;
  uint32_t scale;
  uint8_t bit_index;
  uint8_t col;
  uint8_t top_r[HUB75_WOKWI_WIDTH];
  uint8_t top_g[HUB75_WOKWI_WIDTH];
  uint8_t top_b[HUB75_WOKWI_WIDTH];
  uint8_t bot_r[HUB75_WOKWI_WIDTH];
  uint8_t bot_g[HUB75_WOKWI_WIDTH];
  uint8_t bot_b[HUB75_WOKWI_WIDTH];
} chip_state_t;

static chip_state_t chip;
static pin_watch_config_t clk_watch;
static pin_watch_config_t lat_watch;

static uint8_t expand_bits(uint8_t bits) {
  if (HUB75_WOKWI_COLOR_BITS >= 8) {
    return bits;
  }
  const uint8_t mask = (uint8_t)((1u << HUB75_WOKWI_COLOR_BITS) - 1u);
  bits = (uint8_t)(bits & mask);
  uint8_t expanded = 0;
  for (uint8_t shift = 0; shift < 8; shift += HUB75_WOKWI_COLOR_BITS) {
    expanded = (uint8_t)(expanded | (uint8_t)(bits << shift));
  }
  return expanded;
}

static void fill_display(rgba_t color) {
  for (uint32_t y = 0; y < chip.fb_h; ++y) {
    for (uint32_t x = 0; x < chip.fb_w; ++x) {
      const uint32_t offset = (y * chip.fb_w + x) * 4;
      buffer_write(chip.framebuffer, offset, (uint8_t *)&color, 4);
    }
  }
}

static void write_logical_pixel(uint8_t x, uint8_t y, uint8_t r, uint8_t g,
                                uint8_t b) {
  if (x >= HUB75_WOKWI_WIDTH || y >= HUB75_WOKWI_HEIGHT) {
    return;
  }

  rgba_t pixel = {r, g, b, 255};
  const uint32_t origin_x = (uint32_t)x * chip.scale;
  const uint32_t origin_y = (uint32_t)y * chip.scale;
  for (uint32_t dy = 0; dy < chip.scale; ++dy) {
    for (uint32_t dx = 0; dx < chip.scale; ++dx) {
      const uint32_t offset =
          ((origin_y + dy) * chip.fb_w + (origin_x + dx)) * 4;
      buffer_write(chip.framebuffer, offset, (uint8_t *)&pixel, 4);
    }
  }
}

static void on_clk(void *user_data, pin_t pin, uint32_t value) {
  (void)user_data;
  (void)pin;
  if (value != HIGH) {
    return;
  }
  if (chip.col >= HUB75_WOKWI_WIDTH) {
    return;
  }

  if (chip.bit_index == 0) {
    chip.top_r[chip.col] = 0;
    chip.top_g[chip.col] = 0;
    chip.top_b[chip.col] = 0;
    chip.bot_r[chip.col] = 0;
    chip.bot_g[chip.col] = 0;
    chip.bot_b[chip.col] = 0;
  }

  const uint8_t mask = (uint8_t)(1u << chip.bit_index);
  if (pin_read(chip.r1)) {
    chip.top_r[chip.col] = (uint8_t)(chip.top_r[chip.col] | mask);
  }
  if (pin_read(chip.g1)) {
    chip.top_g[chip.col] = (uint8_t)(chip.top_g[chip.col] | mask);
  }
  if (pin_read(chip.b1)) {
    chip.top_b[chip.col] = (uint8_t)(chip.top_b[chip.col] | mask);
  }
  if (pin_read(chip.r2)) {
    chip.bot_r[chip.col] = (uint8_t)(chip.bot_r[chip.col] | mask);
  }
  if (pin_read(chip.g2)) {
    chip.bot_g[chip.col] = (uint8_t)(chip.bot_g[chip.col] | mask);
  }
  if (pin_read(chip.b2)) {
    chip.bot_b[chip.col] = (uint8_t)(chip.bot_b[chip.col] | mask);
  }

  chip.bit_index++;
  if (chip.bit_index >= HUB75_WOKWI_COLOR_BITS) {
    chip.bit_index = 0;
    chip.col++;
  }
}

static void on_lat(void *user_data, pin_t pin, uint32_t value) {
  (void)user_data;
  (void)pin;
  if (value != HIGH) {
    return;
  }

  uint8_t row = 0;
  if (pin_read(chip.a)) {
    row = (uint8_t)(row | 1u);
  }
  if (pin_read(chip.b)) {
    row = (uint8_t)(row | 2u);
  }
  if (pin_read(chip.c)) {
    row = (uint8_t)(row | 4u);
  }
  if (pin_read(chip.d)) {
    row = (uint8_t)(row | 8u);
  }

  const uint8_t bottom = (uint8_t)(row + (HUB75_WOKWI_HEIGHT / 2));
  const uint8_t columns =
      chip.col > HUB75_WOKWI_WIDTH ? HUB75_WOKWI_WIDTH : chip.col;
  for (uint8_t x = 0; x < columns; ++x) {
    write_logical_pixel(x, row, expand_bits(chip.top_r[x]),
                        expand_bits(chip.top_g[x]), expand_bits(chip.top_b[x]));
    write_logical_pixel(x, bottom, expand_bits(chip.bot_r[x]),
                        expand_bits(chip.bot_g[x]), expand_bits(chip.bot_b[x]));
  }

  chip.col = 0;
  chip.bit_index = 0;
}

void chipInit(void) {
  chip.r1 = pin_init("R1", INPUT);
  chip.g1 = pin_init("G1", INPUT);
  chip.b1 = pin_init("B1", INPUT);
  chip.r2 = pin_init("R2", INPUT);
  chip.g2 = pin_init("G2", INPUT);
  chip.b2 = pin_init("B2", INPUT);
  chip.a = pin_init("A", INPUT);
  chip.b = pin_init("B", INPUT);
  chip.c = pin_init("C", INPUT);
  chip.d = pin_init("D", INPUT);
  chip.clk = pin_init("CLK", INPUT);
  chip.lat = pin_init("LAT", INPUT);
  pin_init("E", INPUT);
  pin_init("OE", INPUT);
  pin_init("GND", INPUT);
  pin_init("VCC", INPUT);

  chip.framebuffer = framebuffer_init(&chip.fb_w, &chip.fb_h);
  chip.scale = chip.fb_w / HUB75_WOKWI_WIDTH;
  if (chip.scale == 0) {
    chip.scale = 1;
  }

  const rgba_t panel = {0, 64, 160, 255};
  fill_display(panel);

  clk_watch.edge = RISING;
  clk_watch.pin_change = on_clk;
  clk_watch.user_data = &chip;
  lat_watch.edge = RISING;
  lat_watch.pin_change = on_lat;
  lat_watch.user_data = &chip;
  pin_watch(chip.clk, &clk_watch);
  pin_watch(chip.lat, &lat_watch);
}
