#include "playback.h"
#undef TRACY_HW_TIMER
#include "tracy/Tracy.hpp"

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace {
long double nanosecondScale() {
  static const long double scale = [] {
    uint64_t const startTime = tracy::Profiler::GetTime();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    uint64_t const endTime = tracy::Profiler::GetTime();
    return (endTime - startTime) / static_cast<long double>(1e9);
  }();
  return scale;
}
} // namespace

template <class... Ts> struct overloads : Ts... {
  using Ts::operator()...;
};
template <typename... Func> overloads(Func...) -> overloads<Func...>;

struct Event {
  uint64_t time;
  uint32_t threadId;
  uint32_t processId;
};

struct EventStart : public Event {
  std::string_view file;
  std::string_view function;
  int line;
};
struct EventEnd : public Event {};

struct Playback::P {
  std::vector<std::variant<EventStart, EventEnd>> stack;

  std::vector<std::thread> playbackThreads;

  P() {
    stack.push_back(EventStart{{0, 1, 1}, "file1.cpp", "function1", 1});
    stack.push_back(
        EventStart{{10000000 / 4, 1, 1}, "file1.cpp", "function1", 1});
    stack.push_back(EventEnd{{10000000 / 4 * 3, 1}});
    stack.push_back(EventEnd{{10000000, 1}});
    stack.push_back(EventStart{{20000000, 2}, "file2.cpp", "function2", 2});
    stack.push_back(EventEnd{{30000000, 2}});
  }
  ~P() {}
};

Playback::Playback() : p(std::make_unique<P>()) {}
Playback::~Playback() {}

void Playback::addFile(const std::filesystem::path &file) {}

void Playback::play() {
  using namespace tracy;
  auto originTime = Profiler::GetTime();

  std::cout << "nanosecondScale: " << nanosecondScale() << std::endl;
  std::cout << "originTime: " << originTime << std::endl;

  static constexpr tracy ::SourceLocationData __tracy_source_location71{
      nullptr, __FUNCTION__,
      "/home/pol/Documentos/tracy-playback/src/playback/src/playback.cpp",
      (uint32_t)71, 0};

  std::chrono::duration startTime =
      std::chrono::high_resolution_clock::now().time_since_epoch();

  for (const auto &event : p->stack) {
    std::visit(
        overloads{
            [originTime, startTime](EventStart const &e) {
              {
                TracyQueuePrepare(QueueType::ZoneBeginAllocSrcLoc);
                auto srcLocation = Profiler::AllocSourceLocation(
                    e.line, e.file.data(), e.file.size(), e.function.data(),
                    e.function.size());
                MemWrite(&item->zoneBegin.time,
                         uint64_t(originTime + e.time * nanosecondScale()));
                MemWrite(&item->zoneBegin.srcloc, srcLocation);
                TracyQueueCommit(zoneBeginThread);
              }
            },
            [originTime, startTime](EventEnd const &e) {
              {
                TracyQueuePrepare(QueueType::ZoneEnd);
                MemWrite(&item->zoneEnd.time,
                         uint64_t(originTime + e.time * nanosecondScale()));
                TracyQueueCommit(zoneEndThread);
              }
            },
        },
        event);
  }
  std::this_thread::sleep_for(std::chrono::seconds(1));
}
