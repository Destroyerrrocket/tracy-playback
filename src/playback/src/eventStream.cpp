#include "eventStream.h"

#include "utilities.h"

namespace TracyPlayback {

EventStream::EventStream(StreamInfo &&stream) : mStream(std::move(stream)) {
  queryNextEvent();
}

std::optional<std::reference_wrapper<TracyRecorder::Event<false> const>>
EventStream::peek() const {
  if (mLastEvent) {
    return std::ref(*mLastEvent);
  }
  return std::nullopt;
}

std::optional<TracyRecorder::Event<false>> EventStream::pop() {
  queryNextEvent();

  if (mLastEvent) {
    auto event = std::move(mLastEvent);
    mLastEvent.reset();
    queryNextEvent();
    return event;
  } else {
    queryNextEvent();
    return std::nullopt;
  }
}

uint64_t EventStream::getNanosecondsSincePosix() const {
  if (mLastEvent) {
    auto &event = *mLastEvent;
    return std::visit(
        overloads{[](TracyRecorder::StartEvent<false> const &event) {
                    return event.unixTime;
                  },
                  [this](auto &event) { return event.time + mStartPosixTime; }},
        event.event);
  }
  return mStartPosixTime;
}

void EventStream::queryNextEvent() {
  if (!mLastEvent && *mStream.first) {
    mLastEvent = TracyRecorder::Event<false>::deserialize(*mStream.first);
  }
  if (mLastEvent) {
    auto &event = *mLastEvent;
    if (auto startEvent =
            std::get_if<TracyRecorder::StartEvent<false>>(&event.event)) {
      mStartPosixTime = startEvent->unixTime;
    }
  }
}

} // namespace TracyPlayback