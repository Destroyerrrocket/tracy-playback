#pragma once

#include "rawEntries.h"

#include <istream>
#include <memory>

namespace TracyPlayback {

class EventStream {
public:
  using StreamInfo = std::pair<std::unique_ptr<std::istream>, std::string>;
  EventStream(StreamInfo &&stream);
  EventStream(EventStream const &) = delete;
  EventStream &operator=(EventStream const &) = delete;
  EventStream(EventStream &&) = default;
  EventStream &operator=(EventStream &&) = default;

  std::optional<std::reference_wrapper<TracyRecorder::Event<false> const>>
  peek() const;
  std::optional<TracyRecorder::Event<false>> pop();

  uint64_t getNanosecondsSincePosix() const;

  std::strong_ordering operator<=>(EventStream const &other) const {
    return getNanosecondsSincePosix() <=> other.getNanosecondsSincePosix();
  }

  std::string_view getStreamName() const { return mStream.second; }

private:
  void queryNextEvent();

  StreamInfo mStream;
  std::optional<TracyRecorder::Event<false>> mLastEvent;
  uint64_t mStartPosixTime = 0;
};

} // namespace TracyPlayback