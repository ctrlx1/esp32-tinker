#include "pixel_art.h"

#include "pixel_art/pixel_art_program.h"

void pixelArtStart(const ProgramConfig &cfg) { PixelArt::start(cfg); }

void pixelArtTick(const ProgramConfig &cfg) { PixelArt::tick(cfg); }
