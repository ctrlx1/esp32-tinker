#pragma once

#include <stddef.h>
#include <stdint.h>
#include <type_traits>

namespace tinker {

using ProgramCallback = void (*)(void *context, uint8_t programIndex);
using TransitionStartCallback = void (*)(void *context);
using TransitionTickCallback = bool (*)(void *context);
using ClockCallback = uint32_t (*)(void *context);

struct ProgramDescriptor {
  const char *id;
  ProgramCallback start;
  ProgramCallback tick;
};

struct SchedulerBindings {
  const ProgramDescriptor *programs;
  void *context;
  ClockCallback nowMs;
  TransitionStartCallback transitionStart;
  TransitionTickCallback transitionTick;
  uint8_t programCount;
};

enum class SchedulerPhase : uint8_t {
  Stopped,
  Running,
  Transitioning,
};

template <typename Mask = uint8_t> class DescriptorScheduler {
  static_assert(std::is_unsigned<Mask>::value,
                "Scheduler mask must be unsigned");

public:
  static constexpr uint8_t kMaskBits = sizeof(Mask) * 8U;

  bool begin(const SchedulerBindings &bindings, Mask selectedPrograms,
             uint8_t fallbackIndex, uint32_t durationMs) {
    if (!bindingsValid(bindings)) {
      reset();
      return false;
    }

    selectedPrograms_ =
        normalizeSelection(selectedPrograms, bindings.programCount,
                           fallbackIndex);
    currentIndex_ = firstSelected(selectedPrograms_, bindings.programCount);
    durationMs_ = durationMs == 0 ? 1 : durationMs;
    programStartMs_ = bindings.nowMs(bindings.context);
    programs_ = bindings.programs;
    context_ = bindings.context;
    programCount_ = bindings.programCount;
    phase_ = SchedulerPhase::Running;
    bindings.programs[currentIndex_].start(bindings.context, currentIndex_);
    return true;
  }

  void tick(const SchedulerBindings &bindings) {
    if (phase_ == SchedulerPhase::Stopped) {
      return;
    }
    if (!bindingsCompatible(bindings)) {
      reset();
      return;
    }

    uint32_t now = bindings.nowMs(bindings.context);
    if (phase_ == SchedulerPhase::Transitioning) {
      if (bindings.transitionTick(bindings.context)) {
        currentIndex_ = nextSelected(selectedPrograms_, bindings.programCount,
                                     currentIndex_);
        phase_ = SchedulerPhase::Running;
        programStartMs_ = bindings.nowMs(bindings.context);
        bindings.programs[currentIndex_].start(bindings.context, currentIndex_);
      }
      return;
    }

    bindings.programs[currentIndex_].tick(bindings.context, currentIndex_);
    if (hasMultipleSelected(selectedPrograms_) &&
        uint32_t(now - programStartMs_) >= durationMs_) {
      bindings.transitionStart(bindings.context);
      phase_ = SchedulerPhase::Transitioning;
    }
  }

  void reset() {
    programStartMs_ = 0;
    durationMs_ = 1;
    selectedPrograms_ = 0;
    currentIndex_ = 0;
    programs_ = nullptr;
    context_ = nullptr;
    programCount_ = 0;
    phase_ = SchedulerPhase::Stopped;
  }

  bool started() const { return phase_ != SchedulerPhase::Stopped; }
  bool transitioning() const {
    return phase_ == SchedulerPhase::Transitioning;
  }
  uint8_t currentIndex() const { return currentIndex_; }
  Mask selectedPrograms() const { return selectedPrograms_; }
  uint32_t durationMs() const { return durationMs_; }

  static Mask validMask(uint8_t programCount) {
    if (programCount >= kMaskBits) {
      return ~Mask(0);
    }
    return (Mask(1) << programCount) - 1;
  }

  static Mask normalizeSelection(Mask selection, uint8_t programCount,
                                 uint8_t fallbackIndex) {
    selection &= validMask(programCount);
    if (selection != 0) {
      return selection;
    }
    if (fallbackIndex >= programCount) {
      fallbackIndex = 0;
    }
    return Mask(1) << fallbackIndex;
  }

private:
  static bool bindingsValid(const SchedulerBindings &bindings) {
    if (!bindings.programs || !bindings.context || !bindings.nowMs ||
        !bindings.transitionStart || !bindings.transitionTick ||
        bindings.programCount == 0 || bindings.programCount > kMaskBits) {
      return false;
    }
    for (uint8_t i = 0; i < bindings.programCount; i++) {
      if (!bindings.programs[i].id || !bindings.programs[i].start ||
          !bindings.programs[i].tick) {
        return false;
      }
    }
    return true;
  }

  bool bindingsCompatible(const SchedulerBindings &bindings) const {
    return bindingsValid(bindings) && bindings.programs == programs_ &&
           bindings.context == context_ &&
           bindings.programCount == programCount_ &&
           currentIndex_ < bindings.programCount;
  }

  static uint8_t firstSelected(Mask selection, uint8_t count) {
    for (uint8_t i = 0; i < count; i++) {
      if (selection & (Mask(1) << i)) {
        return i;
      }
    }
    return 0;
  }

  static uint8_t nextSelected(Mask selection, uint8_t count,
                              uint8_t current) {
    for (uint8_t offset = 1; offset <= count; offset++) {
      uint8_t candidate = (current + offset) % count;
      if (selection & (Mask(1) << candidate)) {
        return candidate;
      }
    }
    return firstSelected(selection, count);
  }

  static bool hasMultipleSelected(Mask selection) {
    return selection != 0 && (selection & (selection - 1)) != 0;
  }

  uint32_t programStartMs_ = 0;
  uint32_t durationMs_ = 1;
  Mask selectedPrograms_ = 0;
  uint8_t currentIndex_ = 0;
  const ProgramDescriptor *programs_ = nullptr;
  void *context_ = nullptr;
  uint8_t programCount_ = 0;
  SchedulerPhase phase_ = SchedulerPhase::Stopped;
};

} // namespace tinker
