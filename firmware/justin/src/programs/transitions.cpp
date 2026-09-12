#include "transitions.h"

#include "program.h"

#include <tinker/display_capabilities.h>
#include <tinker/display_transitions.h>

namespace {

tinker::DisplayTransitions transitions;

MD_MAX72XX *matrix(void *context) {
  return static_cast<MD_Parola *>(context)->getGraphicObject();
}

uint16_t width(void *context) { return matrix(context)->getColumnCount(); }

uint8_t height(void *) { return 8; }

void beginFrame(void *context) {
  matrix(context)->control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
}

void endFrame(void *context) {
  matrix(context)->control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
  matrix(context)->update();
}

void clear(void *context) { matrix(context)->clear(); }

void setPoint(void *context, uint8_t row, uint16_t column, bool on) {
  matrix(context)->setPoint(row, column, on);
}

tinker::FramebufferDisplayCapabilities displayCapabilities() {
  tinker::FramebufferDisplayCapabilities capabilities = {
      &Display, &width, &height, &beginFrame, &endFrame, &clear, &setPoint};
  return capabilities;
}

} // namespace

void transitionStartRandom() { transitions.startRandom(); }

bool transitionTick() {
  return transitions.tick(displayCapabilities());
}
