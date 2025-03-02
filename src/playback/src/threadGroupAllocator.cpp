#include "threadGroupAllocator.h"

#include <tracy/Tracy.hpp>

namespace TracyPlayback {
uint32_t ThreadGroupAllocator::allocate(ProcessInfo const &process) {
  std::lock_guard lock(mMutex);
  auto &nameMap = mMap[process.processId];

  auto it = nameMap.find(process.hostName);
  if (it != nameMap.end()) {
    return it->second;
  }

  static uint32_t nextId = 1;
  return nameMap[process.hostName] = ++nextId;
}
} // namespace TracyPlayback