#pragma once

#include <istream>
#include <memory>

namespace TracyPlayback {
class Playback {
public:
  Playback();
  ~Playback();
  using StreamInfo = std::pair<std::unique_ptr<std::istream>, std::string>;
  void addStream(StreamInfo &&stream);
  void play(bool trace);

private:
  struct P;
  std::unique_ptr<P> p;
};
} // namespace TracyPlayback