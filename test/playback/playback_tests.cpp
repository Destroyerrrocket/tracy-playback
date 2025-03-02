#include "gtest/gtest.h"

#include "playback.h"
#include "rawEntries.h"

#include <istream>
#include <sstream>
#include <vector>

class PlaybackTest : public ::testing::Test {
protected:
  virtual void SetUp() {};

  virtual void TearDown() {};

  std::unique_ptr<std::istream>
  genIStream(std::vector<TracyRecorder::Event<true>> events) {
    std::vector<std::byte> data;
    for (auto &event : events) {
      event.serialize(data);
    }

    auto ss = std::make_unique<std::stringstream>();
    ss->write(reinterpret_cast<const char *>(data.data()), data.size());

    return ss;
  }

  void playStreams(std::vector<std::unique_ptr<std::istream>> streams) {
    TracyPlayback::Playback play;
    for (auto &stream : streams) {
      play.addStream(
          TracyPlayback::Playback::StreamInfo{std::move(stream), ""});
    }
    play.play(true);
  }
};

TEST_F(PlaybackTest, validateWorking) {
  std::vector<TracyRecorder::Event<true>> events = {
      TracyRecorder::Event(
          TracyRecorder::StartEvent<true>("host", 1234567890, 42)),
      TracyRecorder::Event(TracyRecorder::StartZoneEvent<true>(
          0, 1, "file1.cpp", "function1", "name1", 0, 100)),
      TracyRecorder::Event(TracyRecorder::EndZoneEvent<true>(0, 200)),
      TracyRecorder::Event(
          TracyRecorder::MessageEvent<true>("message1", 1234, 0, 300)),
      TracyRecorder::Event(
          TracyRecorder::ThreadNameEvent<true>("thread1", 0, 400))};

  std::vector<std::unique_ptr<std::istream>> streams;
  streams.push_back(genIStream(events));
  playStreams(std::move(streams));
}

TEST_F(PlaybackTest, validateMultipleStreams) {
  std::vector<std::vector<TracyRecorder::Event<true>>> events = {
      {TracyRecorder::Event(
           TracyRecorder::StartEvent<true>("host1", 1234567890, 42)),
       TracyRecorder::Event(TracyRecorder::StartZoneEvent<true>(
           0, 1, "file1.cpp", "function1", "name1", 0, 100)),
       TracyRecorder::Event(TracyRecorder::StartZoneEvent<true>(
           0, 1, "file1.cpp", "function1", "name1", 1, 150)),
       TracyRecorder::Event(TracyRecorder::EndZoneEvent<true>(0, 200)),
       TracyRecorder::Event(TracyRecorder::EndZoneEvent<true>(1, 250)),
       TracyRecorder::Event(
           TracyRecorder::MessageEvent<true>("message1", 1234, 0, 300)),
       TracyRecorder::Event(
           TracyRecorder::MessageEvent<true>("message1", 1234, 1, 350)),
       TracyRecorder::Event(
           TracyRecorder::ThreadNameEvent<true>("thread1", 0, 400)),
       TracyRecorder::Event(
           TracyRecorder::ThreadNameEvent<true>("thread2", 1, 400))},
      {TracyRecorder::Event(
           TracyRecorder::StartEvent<true>("host2", 1234567890, 42)),
       TracyRecorder::Event(TracyRecorder::StartZoneEvent<true>(
           0, 1, "file1.cpp", "function1", "name1", 0, 100)),
       TracyRecorder::Event(TracyRecorder::EndZoneEvent<true>(0, 200)),
       TracyRecorder::Event(
           TracyRecorder::MessageEvent<true>("message1", 1234, 0, 300)),
       TracyRecorder::Event(
           TracyRecorder::ThreadNameEvent<true>("thread3", 0, 400))},
  };

  std::vector<std::unique_ptr<std::istream>> streams;
  for (auto &event : events) {
    streams.push_back(genIStream(event));
  }
  playStreams(std::move(streams));
}