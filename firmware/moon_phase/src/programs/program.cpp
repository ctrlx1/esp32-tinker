#include "program.h"

#include <cassert>

namespace {
tinker::RuntimeContext *runtimeContext = nullptr;
}

void setProgramRuntimeContext(tinker::RuntimeContext &runtime) {
  assert(runtimeContext == nullptr || runtimeContext == &runtime);
  runtimeContext = &runtime;
}

tinker::RuntimeContext &programRuntimeContext() {
  assert(runtimeContext != nullptr);
  return *runtimeContext;
}
