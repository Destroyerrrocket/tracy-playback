#include "playback.h"

#include "eventStream.h"
#include "playbackThread.h"
#include "processInfo.h"

#include "tracy/Tracy.hpp"
#include "utilities.h"

#include <format>
#include <iostream>
#include <memory>
#include <queue>
#include <string>
#include <unordered_map>
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

namespace TracyPlayback {

struct Playback::P {
  std::unordered_map<
      std::string,
      std::unordered_map<
          uint64_t,
          std::unordered_map<uint64_t, std::unique_ptr<PlaybackThread>>>>
      playbackThreads;

  using StreamWithInfo =
      std::shared_ptr<std::pair<EventStream, ProcessInfo>>; // Could have been a
                                                            // unique ptr

  struct CompareStreamWithInfo {
    bool operator()(StreamWithInfo const &a, StreamWithInfo const &b) {
      return a->first.getNanosecondsSincePosix() >
             b->first.getNanosecondsSincePosix();
    }
  };

  std::priority_queue<StreamWithInfo, std::vector<StreamWithInfo>,
                      CompareStreamWithInfo>
      eventStreams;

  uint64_t minimumUnixTime = std::numeric_limits<uint64_t>::max();

  P() = default;
  ~P() = default;

  void addStream(StreamInfo &&stream) {
    EventStream events{std::move(stream)};
    auto event = events.pop();
    if (event.has_value()) {
      if (auto startEvent =
              std::get_if<TracyRecorder::StartEvent<false>>(&event->event)) {
        minimumUnixTime = std::min(minimumUnixTime, startEvent->unixTime);
        ProcessInfo processInfo{std::move(startEvent->host),
                                startEvent->processId};

        std::cout << std::format("Added stream from host '{}' PID '{}'",
                                 processInfo.hostName, processInfo.processId)
                  << std::endl;

        eventStreams.emplace(
            std::make_shared<std::pair<EventStream, ProcessInfo>>(
                std::move(events), std::move(processInfo)));

        return;
      }
    }

    std::cout << std::format("FAILED to add stream") << std::endl;
  }

  PlaybackThread &getOrEmplace(ProcessInfo const &processInfo,
                               uint64_t threadId) {
    auto &processes = playbackThreads[processInfo.hostName];
    auto &threads = processes[processInfo.processId];

    auto it = threads.find(threadId);
    if (it == threads.end()) {
      it = threads
               .try_emplace(threadId, std::make_unique<PlaybackThread>(
                                          processInfo, threadId))
               .first;
    }
    return *it->second;
  }
};

Playback::Playback() : p(std::make_unique<P>()) {}
Playback::~Playback() = default;

void Playback::addStream(StreamInfo &&stream) {
  p->addStream(std::move(stream));
}

void Playback::play(bool trace) {
  using namespace tracy;
  auto originTime = Profiler::GetTime();

  std::cout << "nanosecondScale: " << nanosecondScale() << std::endl;
  std::cout << "originTime: " << originTime << std::endl;

  while (!p->eventStreams.empty()) {
    auto eventStream = p->eventStreams.top();
    p->eventStreams.pop();

    auto eventTime = eventStream->first.getNanosecondsSincePosix();
    if (auto event = eventStream->first.pop(); event.has_value()) {
      auto eventType = event->type();

      auto threadId = std::visit(
          overloads{[](TracyRecorder::StartEvent<false> const &e) -> uint64_t {
                      std::cout << "StartEvent in event stream is "
                                   "unexpected. Start event "
                                   "should only happen once at the start "
                                   "of the stream"
                                << std::endl;

                      throw std::runtime_error("StartEvent in event stream is "
                                               "unexpected. Start event "
                                               "should only happen once at the "
                                               "start of the stream");
                    },
                    [](auto const &e) -> uint64_t { return e.threadId; }},
          event->event);

      auto &thread = p->getOrEmplace(eventStream->second, threadId);
      thread.submitEvent(
          std::move(*event),
          uint64_t(originTime +
                   (eventTime - p->minimumUnixTime) * nanosecondScale()));

      if (trace) {
        std::cout << std::format(
            "Event for host '{}' PID '{}' TID '{}' Event type {}\n",
            eventStream->second.hostName, eventStream->second.processId,
            threadId,
            static_cast<std::underlying_type_t<TracyRecorder::EventType>>(
                eventType));
      }
      if (!eventStream->first.peek().has_value()) {
        std::cout << std::format(
            "Host '{}' PID '{}' DONE!\n", eventStream->second.hostName,
            eventStream->second.processId,
            static_cast<std::underlying_type_t<TracyRecorder::EventType>>(
                eventType));
      }
      p->eventStreams.push(eventStream);
    }
  }
}
} // namespace TracyPlayback
