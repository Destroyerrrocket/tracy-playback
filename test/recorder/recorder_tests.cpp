#include "gtest/gtest.h"

#include "rawEntries.h"
#include "recorder.h"
#include "utilities.h"

using namespace std;

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
} // namespace

class RecorderTest : public ::testing::Test {

protected:
  virtual void SetUp() {
    TracyRecorder::setFlushCallback([this](std::vector<std::byte> const &p) {
      std::vector<std::byte> p2(p);
      if (count == 0) {
        p2.erase(p2.begin(), p2.begin() + 12);
      }
      count++;
      output.push_back(std::move(p2));
    });
  };

  virtual void TearDown() {};

  bool compareIgnoreTime(TracyRecorder::Event<false> &a,
                         TracyRecorder::Event<false> &b) {
    auto eraseTime =
        overloads{[](auto &event) { event.time = 0; },
                  []<bool isOut>(TracyRecorder::StartEvent<isOut> &) {}};

    std::visit(eraseTime, a.event);
    std::visit(eraseTime, b.event);
    return a == b;
  }

  void testEvent(std::vector<TracyRecorder::Event<false>> events) {
    std::stringstream strstream(
        std::string(reinterpret_cast<const char *>(output.back().data()),
                    output.back().size()),
        std::ios::in | std::ios::out | std::ios::binary);

    auto it = events.begin();
    while (strstream) {
      auto event = TracyRecorder::Event<false>::deserialize(strstream);
      if (event.has_value()) {
        if (it == events.end()) {
          FAIL() << "Too many events";
        }
        ASSERT_TRUE(compareIgnoreTime(*it, *event));
        ++it;
      }
    }
  }

  std::vector<TracyRecorder::Event<false>> getLastEvents() {
    std::stringstream strstream(
        std::string(reinterpret_cast<const char *>(output.back().data()),
                    output.back().size()),
        std::ios::in | std::ios::out | std::ios::binary);

    std::vector<TracyRecorder::Event<false>> events;
    while (strstream) {
      auto event = TracyRecorder::Event<false>::deserialize(strstream);
      if (event.has_value()) {
        events.push_back(std::move(*event));
      }
    }
    return events;
  }

  size_t count = 0;
  std::vector<std::vector<std::byte>> output;
};

TEST_F(RecorderTest, testStartEvent) {
  auto events = getLastEvents();
  EXPECT_EQ(events.size(), 1);

  EXPECT_EQ(events[0].type(), TracyRecorder::EventType::Start);
  auto startEvent = std::get<TracyRecorder::StartEvent<false>>(events[0].event);

  EXPECT_EQ(startEvent.host, getHostName());
  EXPECT_EQ(startEvent.processId, getpid());
  EXPECT_LE(startEvent.unixTime,
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count());
  EXPECT_LE(std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                    .count() -
                startEvent.unixTime,
            60LL * 1000000000);
}

TEST_F(RecorderTest, testZoneStartEvent) {
  TracyRecorder::zoneStart(1, "file1.cpp", "function1", "name1", 0);
  TracyRecorder::flush();

  testEvent({TracyRecorder::Event(TracyRecorder::StartZoneEvent<false>(
      0, 1, "file1.cpp", "function1", "name1",
      std::bit_cast<uint64_t>(std::this_thread::get_id()), 0))});
}

TEST_F(RecorderTest, testZoneEndEvent) {
  TracyRecorder::zoneEnd();
  TracyRecorder::flush();

  testEvent({TracyRecorder::Event(TracyRecorder::EndZoneEvent<false>(
      std::bit_cast<uint64_t>(std::this_thread::get_id()), 0))});
}

TEST_F(RecorderTest, testMessageEvent) {
  TracyRecorder::message("message1", 1234);
  TracyRecorder::flush();

  testEvent({TracyRecorder::Event(TracyRecorder::MessageEvent<false>(
      "message1", 1234, std::bit_cast<uint64_t>(std::this_thread::get_id()),
      0))});
}

TEST_F(RecorderTest, testThreadNameEvent) {
  TracyRecorder::nameThread("thread1");
  TracyRecorder::flush();

  testEvent({TracyRecorder::Event(TracyRecorder::ThreadNameEvent<false>(
      "thread1", std::bit_cast<uint64_t>(std::this_thread::get_id()), 0))});
}

TEST_F(RecorderTest, testMultipleEvents) {
  TracyRecorder::zoneStart(1, "file1.cpp", "function1", "name1", 0);
  TracyRecorder::zoneEnd();
  TracyRecorder::message("message1", 1234);
  TracyRecorder::nameThread("thread1");
  TracyRecorder::flush();

  testEvent({TracyRecorder::Event(TracyRecorder::StartZoneEvent<false>(
                 0, 1, "file1.cpp", "function1", "name1",
                 std::bit_cast<uint64_t>(std::this_thread::get_id()), 0)),
             TracyRecorder::Event(TracyRecorder::EndZoneEvent<false>(
                 std::bit_cast<uint64_t>(std::this_thread::get_id()), 0)),
             TracyRecorder::Event(TracyRecorder::MessageEvent<false>(
                 "message1", 1234,
                 std::bit_cast<uint64_t>(std::this_thread::get_id()), 0)),
             TracyRecorder::Event(TracyRecorder::ThreadNameEvent<false>(
                 "thread1", std::bit_cast<uint64_t>(std::this_thread::get_id()),
                 0))});
}