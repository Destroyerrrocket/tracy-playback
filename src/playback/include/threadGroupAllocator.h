#pragma once

#include "processInfo.h"

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

namespace TracyPlayback {
struct ThreadGroupAllocator {
public:
  uint32_t allocate(ProcessInfo const &process);

private:
  template <class A, class B> using Map = std::unordered_map<A, B>;
  using MapName = Map<std::string, uint32_t>;
  using MapProcess = Map<uint32_t, MapName>;

  std::mutex mMutex;
  MapProcess mMap;
};
} // namespace TracyPlayback