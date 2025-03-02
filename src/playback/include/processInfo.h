#pragma once

#include <cstdint>
#include <string>

namespace TracyPlayback {
struct ProcessInfo {
  std::string hostName;
  uint64_t processId;
};
} // namespace TracyPlayback