#include "playbackThread.h"

#include "threadGroupAllocator.h"
#include "utilities.h"

#include <format>
#include <iostream>
#include <tracy/Tracy.hpp>

namespace TracyPlayback {
namespace {
ThreadGroupAllocator &getThreadGroupAllocator() {
  static ThreadGroupAllocator allocator;
  return allocator;
}
} // namespace

PlaybackThread::PlaybackThread(ProcessInfo const &processInfo,
                               uint64_t threadId) {
  mThread =
      std::jthread(&PlaybackThread::threadFunc, this, processInfo, threadId);
}

bool PlaybackThread::handleEvent(ProcessInfo const &processInfo,
                                 TracyRecorder::Event<false> const &event,
                                 uint64_t adjustedTime) {
  using namespace tracy;
  bool nameSetExplicitly = false;

  std::visit(
      overloads{
          [adjustedTime](TracyRecorder::StartEvent<false> const &e) {
            std::cout << "Unexpected StartEvent\n";
          },
          [adjustedTime](TracyRecorder::StartZoneEvent<false> const &e) {
            TracyQueuePrepare(QueueType::ZoneBeginAllocSrcLoc);
            auto srcLocation = tracy::Profiler::AllocSourceLocation(
                e.line, e.file.data(), e.file.size(), e.function.data(),
                e.function.size(), e.name.data(), e.name.size(), e.color);
            MemWrite(&item->zoneBegin.time, adjustedTime);
            MemWrite(&item->zoneBegin.srcloc, srcLocation);
            TracyQueueCommit(zoneBeginThread);
          },
          [adjustedTime](TracyRecorder::EndZoneEvent<false> const &e) {
            TracyQueuePrepare(QueueType::ZoneEnd);
            MemWrite(&item->zoneEnd.time, adjustedTime);
            TracyQueueCommit(zoneEndThread);
          },
          [adjustedTime](TracyRecorder::MessageEvent<false> const &e) {
            auto ptr = (char *)tracy_malloc(e.message.size());
            memcpy(ptr, e.message.data(), e.message.size());

            if (e.message.size() > std::numeric_limits<uint16_t>::max()) {
              std::cout << std::format("Message too long: {}\n", e.message);
              return;
            }

            if (e.color == 0) {
              TracyQueuePrepare(QueueType::Message);
              MemWrite(&item->messageFat.time, adjustedTime);
              MemWrite(&item->messageFat.text, (uint64_t)ptr);
              MemWrite(&item->messageFat.size, (uint16_t)e.message.size());
              TracyQueueCommit(messageFatThread);
            } else {
              TracyQueuePrepare(QueueType::MessageColor);
              MemWrite(&item->messageColorFat.time, adjustedTime);
              MemWrite(&item->messageColorFat.text, (uint64_t)ptr);
              MemWrite(&item->messageColorFat.b, uint8_t((e.color) & 0xFF));
              MemWrite(&item->messageColorFat.g,
                       uint8_t((e.color >> 8) & 0xFF));
              MemWrite(&item->messageColorFat.r,
                       uint8_t((e.color >> 16) & 0xFF));
              MemWrite(&item->messageColorFat.size, (uint16_t)e.message.size());
              TracyQueueCommit(messageColorFatThread);
            }
          },
          [adjustedTime, &nameSetExplicitly,
           &processInfo](TracyRecorder::ThreadNameEvent<false> const &e) {
            auto newName =
                std::format("{}: {}_{}_{}", e.name, processInfo.hostName,
                            processInfo.processId, e.threadId);
            SetThreadNameWithHint(
                newName.c_str(),
                getThreadGroupAllocator().allocate(processInfo));
            nameSetExplicitly = true;
          }},
      event.event);

  return nameSetExplicitly;
}

void PlaybackThread::threadFunc(std::stop_token stopToken,
                                ProcessInfo processInfo, uint64_t threadId) {
  bool nameSetExplicitly = false;

  while (!stopToken.stop_requested()) {
    std::unique_lock<std::mutex> lock(mMutexRecieveNextEvent);
    mCondReceiveNextEvent.wait(lock, stopToken,
                               [this] { return mNextEvent.has_value(); });

    if (mNextEvent) {
      auto [event, adjustedTime] = *mNextEvent;
      mNextEvent.reset();

      nameSetExplicitly |= handleEvent(processInfo, event, adjustedTime);

      std::unique_lock<std::mutex> lock(mMutexProcessedNextEvent);
      mProcessedNextEvent += 1;
      mCondProcessedNextEvent.notify_one();
    }
  }

  if (!nameSetExplicitly) {
    auto newName = std::format("{}_{}_{}", processInfo.hostName,
                               processInfo.processId, threadId);
    tracy::SetThreadNameWithHint(
        newName.c_str(), getThreadGroupAllocator().allocate(processInfo));
  }
}

void PlaybackThread::submitEvent(TracyRecorder::Event<false> event,
                                 uint64_t adjustedTime) {
  std::unique_lock<std::mutex> lock(mMutexProcessedNextEvent);
  uint64_t myEvent = mProcessedNextEvent + 1;
  {
    std::scoped_lock lock(mMutexRecieveNextEvent);
    mNextEvent = std::make_pair(std::move(event), adjustedTime);
    mCondReceiveNextEvent.notify_one();
  }

  mCondProcessedNextEvent.wait(
      lock, [this, myEvent] { return mProcessedNextEvent >= myEvent; });
}

} // namespace TracyPlayback
