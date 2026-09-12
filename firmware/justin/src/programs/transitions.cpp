#include "transitions.h"

#include "program.h"

#include <tinker/display_transitions.h>

namespace {
tinker::DisplayTransitions transitions;
}

void transitionStartRandom() { transitions.startRandom(); }

bool transitionTick() {
  return transitions.tick(programRuntimeContext());
}
