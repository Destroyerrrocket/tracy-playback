#pragma once

#include "processInfo.h"
#include "rawEntries.h"

#include <condition_variable>
#include <mutex>
#include <thread>

namespace TracyPlayback {
class PlaybackThread {
public:
  PlaybackThread(ProcessInfo const &processInfo, uint64_t threadId);

  void submitEvent(TracyRecorder::Event<false> event, uint64_t adjustedTime);

private:
  void threadFunc(std::stop_token stopToken, ProcessInfo processInfo,
                  uint64_t threadId);
  bool handleEvent(ProcessInfo const &processInfo,
                   TracyRecorder::Event<false> const &event,
                   uint64_t adjustedTime);

  std::condition_variable_any mCondReceiveNextEvent;
  std::mutex mMutexRecieveNextEvent;
  std::optional<std::pair<TracyRecorder::Event<false>, uint64_t>> mNextEvent;

  std::condition_variable mCondProcessedNextEvent;
  std::mutex mMutexProcessedNextEvent;
  std::uint64_t mProcessedNextEvent = 0;

  std::jthread mThread;
};

} // namespace TracyPlayback