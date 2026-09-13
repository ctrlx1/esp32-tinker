import shutil
import subprocess
import tempfile
import textwrap
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
CORE_INCLUDE = ROOT / "packages" / "tinker-core" / "src"


class DescriptorSchedulerTests(unittest.TestCase):
    def test_scheduler_contract(self) -> None:
        compiler = shutil.which("c++")
        if compiler is None:
            self.skipTest("A host C++ compiler is not available")

        source = textwrap.dedent(
            r"""
            #include <assert.h>
            #include <stdint.h>
            #include <string>
            #include <vector>

            #include <tinker/descriptor_scheduler.h>

            struct Context {
              uint32_t now = 0;
              bool transitionDone = false;
              std::vector<std::string> events;
            };

            uint32_t nowMs(void *raw) {
              return static_cast<Context *>(raw)->now;
            }

            void start(void *raw, uint8_t index) {
              static_cast<Context *>(raw)->events.push_back(
                  "start" + std::to_string(index));
            }

            void tick(void *raw, uint8_t index) {
              static_cast<Context *>(raw)->events.push_back(
                  "tick" + std::to_string(index));
            }

            void transitionStart(void *raw) {
              static_cast<Context *>(raw)->events.push_back("transitionStart");
            }

            bool transitionTick(void *raw) {
              Context *context = static_cast<Context *>(raw);
              context->events.push_back("transitionTick");
              return context->transitionDone;
            }

            int main() {
              const tinker::ProgramDescriptor programs[] = {
                  {"zero", start, tick},
                  {"one", start, tick},
                  {"two", start, tick},
              };
              Context context;
              const tinker::SchedulerBindings bindings = {
                  programs, &context, nowMs, transitionStart, transitionTick, 3};
              tinker::DescriptorScheduler<uint8_t> scheduler;

              assert(scheduler.begin(bindings, 0, 2, 10));
              assert(scheduler.currentIndex() == 2);
              assert(scheduler.selectedPrograms() == 0b100);
              assert(context.events.back() == "start2");

              scheduler.reset();
              context.events.clear();
              context.now = 100;
              assert(scheduler.begin(bindings, 0b101, 0, 10));
              context.now = 110;
              scheduler.tick(bindings);
              assert(context.events[0] == "start0");
              assert(context.events[1] == "tick0");
              assert(context.events[2] == "transitionStart");
              assert(scheduler.transitioning());

              scheduler.tick(bindings);
              assert(context.events.back() == "transitionTick");
              assert(scheduler.currentIndex() == 0);

              context.transitionDone = true;
              context.now = 115;
              scheduler.tick(bindings);
              assert(context.events[context.events.size() - 2] ==
                     "transitionTick");
              assert(context.events.back() == "start2");
              assert(scheduler.currentIndex() == 2);
              assert(!scheduler.transitioning());

              Context otherContext;
              const tinker::SchedulerBindings changedBindings = {
                  programs, &otherContext, nowMs, transitionStart,
                  transitionTick, 3};
              scheduler.tick(changedBindings);
              assert(!scheduler.started());
              assert(otherContext.events.empty());

              context.events.clear();
              context.now = 1000;
              assert(scheduler.begin(bindings, 0b010, 1, 1));
              context.now = 5000;
              scheduler.tick(bindings);
              assert(context.events.size() == 2);
              assert(context.events[0] == "start1");
              assert(context.events[1] == "tick1");
              assert(!scheduler.transitioning());

              scheduler.reset();
              context.events.clear();
              context.now = UINT32_MAX - 5;
              assert(scheduler.begin(bindings, 0b011, 0, 10));
              context.now = 5;
              scheduler.tick(bindings);
              assert(scheduler.transitioning());

              assert(tinker::DescriptorScheduler<uint8_t>::validMask(3) ==
                     0b111);
              assert(tinker::DescriptorScheduler<uint8_t>::validMask(8) ==
                     0xff);
              return 0;
            }
            """
        )

        with tempfile.TemporaryDirectory() as temp_dir:
            temp = Path(temp_dir)
            source_path = temp / "scheduler_test.cpp"
            binary_path = temp / "scheduler_test"
            source_path.write_text(source, encoding="utf-8")
            subprocess.run(
                [
                    compiler,
                    "-std=c++11",
                    "-I",
                    str(CORE_INCLUDE),
                    str(source_path),
                    "-o",
                    str(binary_path),
                ],
                check=True,
                capture_output=True,
                text=True,
            )
            subprocess.run([str(binary_path)], check=True)


if __name__ == "__main__":
    unittest.main()
