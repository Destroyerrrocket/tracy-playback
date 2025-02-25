#include "recorder.h"

#include "rawEntries.h"

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stop_token>
#include <thread>

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

namespace TracyRecorder {
namespace {
std::string_view getHostName() {
  static std::string const hostname = [] {
    std::string hostname;
#ifdef _WIN32
    std::array<char, MAX_COMPUTERNAME_LENGTH> buffer;
    if (GetComputerNameA(buffer.data(), buffer.size())) {
      hostname = buffer.data();
    } else {
      hostname = "unknown";
    }
#else
    std::array<char, 256> buffer;
    if (gethostname(buffer.data(), buffer.size()) == 0) {
      hostname = buffer.data();
    } else {
      hostname = "unknown";
    }
#endif
    return hostname;
  }();
  return hostname;
}

struct ReferenceClocks {
  ReferenceClocks() {
    globalTime = std::chrono::duration_cast<std::chrono::nanoseconds>(
                     std::chrono::system_clock::now().time_since_epoch())
                     .count();
    referenceStart = std::chrono::high_resolution_clock::now();
  }

  uint64_t globalTime;
  std::chrono::high_resolution_clock::time_point referenceStart;
} globalReferenceClocks;

std::vector<std::byte> toByteVector(const char *data, size_t size) {
  return std::vector<std::byte>(
      reinterpret_cast<const std::byte *>(data),
      reinterpret_cast<const std::byte *>(data + size));
}

class GlobalRecorder {
public:
  GlobalRecorder() { mData.reserve(1024); };

  ~GlobalRecorder() {
    if (flushThread.joinable()) {
      flushThread.request_stop();
    }
    mCondGlobalData.notify_one();
  };

  void flush(uint64_t upToValue) {
    std::unique_lock<std::mutex> lock(mMutexCounterFlushed);
    mCondCounterFlushed.wait(
        lock, [this, upToValue] { return mCounterFlushed >= upToValue; });
  }

  void
  setOutput(const std::function<void(std::vector<std::byte> const &)> &output) {
    mOutput = output;

    std::string_view header = "TRCYPLAY\1\0\0\0";
    auto startMessage = toByteVector(header.data(), 12);
    Event(StartEvent<true>(getHostName(), globalReferenceClocks.globalTime,
                           getpid()))
        .serialize(startMessage);
    mOutput(std::move(startMessage));

    flushThread = std::jthread(&GlobalRecorder::flushThreadFunc, this);
  }

  uint64_t sendRecord(std::vector<Event<true>> &data) {
    uint64_t size = data.size();
    std::scoped_lock lock(mMutexGlobalData);

    if (mData.empty()) {
      std::swap(mData, data);
    } else {
      mData.insert(mData.end(), data.begin(), data.end());
      data.clear();
    }

    mCondGlobalData.notify_one();
    return mCounterSubmitted += size;
  }

private:
  void flushThreadFunc(std::stop_token stop_token) {
    std::vector<Event<true>> data;
    data.reserve(1024);
    std::vector<std::byte> rawMessage;
    rawMessage.reserve(1024 * 128);

    while (!stop_token.stop_requested()) {
      {
        std::unique_lock<std::mutex> lock(mMutexGlobalData);
        mCondGlobalData.wait(lock, stop_token,
                             [&stop_token, this] { return !mData.empty(); });
        std::swap(data, mData);
      }

      if (data.empty()) {
        continue;
      }

      uint64_t size = data.size();
      for (auto &event : data) {
        event.serialize(rawMessage);
      }
      mOutput(rawMessage);
      data.clear();
      rawMessage.clear();

      {
        std::scoped_lock lock(mMutexCounterFlushed);
        mCounterFlushed += size;
        mCondCounterFlushed.notify_one();
      }
    }
  }
  std::function<void(std::vector<std::byte> const &)> mOutput;

  std::mutex mMutexGlobalData;
  std::condition_variable_any mCondGlobalData;
  std::vector<Event<true>> mData;
  uint64_t mCounterSubmitted = 0;

  std::mutex mMutexCounterFlushed;
  std::condition_variable mCondCounterFlushed;
  uint64_t mCounterFlushed = 0;

  // Keep last, we want to finish this thread before destroying the main object
  std::jthread flushThread;
};

GlobalRecorder &getGlobalRecorder() {
  static GlobalRecorder recorder;
  return recorder;
}

class LocalRecorder {
public:
  LocalRecorder() { mData.reserve(1024); };

  ~LocalRecorder() { flush(); };

  void flush() {
    auto &global = getGlobalRecorder();
    global.flush(global.sendRecord(mData));
  }

  void zoneBegin(uint32_t line, std::string_view file,
                 std::string_view function, std::string_view name,
                 uint32_t color) {
    mData.push_back(TracyRecorder::StartZoneEvent<true>(
        color, line, file, function, name,
        std::bit_cast<uint64_t>(std::this_thread::get_id()),
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now() -
            globalReferenceClocks.referenceStart)
            .count()));
  }

  void zoneEnd() {
    mData.push_back(TracyRecorder::EndZoneEvent<true>(
        std::bit_cast<uint64_t>(std::this_thread::get_id()),
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now() -
            globalReferenceClocks.referenceStart)
            .count()));
  }

  void nameThread(std::string_view name) {
    mData.push_back(TracyRecorder::ThreadNameEvent<true>(
        name, std::bit_cast<uint64_t>(std::this_thread::get_id()),
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now() -
            globalReferenceClocks.referenceStart)
            .count()));
  }

  void message(std::string_view message) {
    mData.push_back(TracyRecorder::MessageEvent<true>(
        message, std::bit_cast<uint64_t>(std::this_thread::get_id()),
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now() -
            globalReferenceClocks.referenceStart)
            .count()));
  }

public:
  std::vector<Event<true>> mData;
  uint32_t mZoneDepth = 0;
};

thread_local LocalRecorder localRecorder;
} // namespace

void setFlushCallback(
    const std::function<void(std::vector<std::byte> const &)> &output) {
  getGlobalRecorder().setOutput(output);
}

void flush() { localRecorder.flush(); }

void nameThread(std::string_view name) { localRecorder.nameThread(name); }

void zoneStart(uint32_t line, std::string_view file, std::string_view function,
               std::string_view name, uint32_t color) {
  localRecorder.zoneBegin(line, file, function, name, color);
}
void zoneEnd() { localRecorder.zoneEnd(); }

void message(std::string_view message) { localRecorder.message(message); }
} // namespace TracyRecorder