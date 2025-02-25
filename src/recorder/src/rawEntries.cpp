#include "rawEntries.h"

#include <cstring>
namespace {
template <class T>
concept Number =
    std::integral<T> || std::floating_point<T> || std::is_enum_v<T>;

void serializeRaw(std::vector<std::byte> &out, Number auto rawData) {
  size_t dataSize = sizeof(rawData);
  out.resize(out.size() + dataSize);
  std::memcpy(out.data() + out.size() - dataSize, &rawData, dataSize);
}

void serializeRaw(std::vector<std::byte> &out, std::string_view rawData) {
  uint64_t dataSize = rawData.size();
  serializeRaw(out, dataSize);

  out.resize(out.size() + dataSize);
  std::memcpy(out.data() + out.size() - dataSize, rawData.data(), dataSize);
}

#define DESERIALIZE_RAW(localVariable)                                         \
  {                                                                            \
    auto opt = deserializeRaw<decltype(localVariable)>(data);                  \
    if (!opt) {                                                                \
      return std::nullopt;                                                     \
    }                                                                          \
    localVariable = *opt;                                                      \
  }                                                                            \
  static_cast<void>(0)

template <class T> std::optional<T> deserializeRaw(std::istream &data) {
  static_assert(
      requires { requires Number<T>; },
      "Overload requires T to be a raw number");
  T value;
  data.read(reinterpret_cast<char *>(&value), sizeof(T));
  if (data.gcount() != sizeof(T)) {
    return std::nullopt;
  }
  return value;
}

template <>
std::optional<std::string> deserializeRaw<std::string>(std::istream &data) {
  uint64_t dataSize;
  DESERIALIZE_RAW(dataSize);

  std::string result(dataSize, '\0');
  data.read(result.data(), dataSize);
  if (!data || data.gcount() != dataSize) {
    return std::nullopt;
  }

  return result;
}
} // namespace

namespace TracyRecorder {

template <>
void EventHeader<EventType::Start, StartEvent<true>, true>::serialize(
    StartEvent<true> const &self, std::vector<std::byte> &out) {
  serializeRaw(out, self.host);
  serializeRaw(out, self.unixTime);
  serializeRaw(out, self.processId);
}

template <>
std::optional<StartEvent<false>>
EventHeader<EventType::Start, StartEvent<false>, false>::deserialize(
    std::istream &data) {
  StartEvent<false> event;
  DESERIALIZE_RAW(event.host);
  DESERIALIZE_RAW(event.unixTime);
  DESERIALIZE_RAW(event.processId);
  return event;
}

template <>
void EventHeader<EventType::StartZone, StartZoneEvent<true>, true>::serialize(
    StartZoneEvent<true> const &self, std::vector<std::byte> &out) {
  serializeRaw(out, self.time);
  serializeRaw(out, self.threadId);
  serializeRaw(out, self.file);
  serializeRaw(out, self.function);
  serializeRaw(out, self.name);
  serializeRaw(out, self.line);
  serializeRaw(out, self.color);
}

template <>
std::optional<StartZoneEvent<false>>
EventHeader<EventType::StartZone, StartZoneEvent<false>, false>::deserialize(
    std::istream &data) {
  StartZoneEvent<false> event;
  DESERIALIZE_RAW(event.time);
  DESERIALIZE_RAW(event.threadId);
  DESERIALIZE_RAW(event.file);
  DESERIALIZE_RAW(event.function);
  DESERIALIZE_RAW(event.name);
  DESERIALIZE_RAW(event.line);
  DESERIALIZE_RAW(event.color);
  return event;
}

template <>
void EventHeader<EventType::EndZone, EndZoneEvent<true>, true>::serialize(
    EndZoneEvent<true> const &self, std::vector<std::byte> &out) {
  serializeRaw(out, self.time);
  serializeRaw(out, self.threadId);
}

template <>
std::optional<EndZoneEvent<false>>
EventHeader<EventType::EndZone, EndZoneEvent<false>, false>::deserialize(
    std::istream &data) {
  EndZoneEvent<false> event;
  DESERIALIZE_RAW(event.time);
  DESERIALIZE_RAW(event.threadId);
  return event;
}

template <>
void EventHeader<EventType::Message, MessageEvent<true>, true>::serialize(
    MessageEvent<true> const &self, std::vector<std::byte> &out) {
  serializeRaw(out, self.time);
  serializeRaw(out, self.threadId);
  serializeRaw(out, self.message);
}

template <>
std::optional<MessageEvent<false>>
EventHeader<EventType::Message, MessageEvent<false>, false>::deserialize(
    std::istream &data) {
  MessageEvent<false> event;
  DESERIALIZE_RAW(event.time);
  DESERIALIZE_RAW(event.threadId);
  DESERIALIZE_RAW(event.message);
  return event;
}

template <>
void EventHeader<EventType::ThreadName, ThreadNameEvent<true>, true>::serialize(
    ThreadNameEvent<true> const &self, std::vector<std::byte> &out) {
  serializeRaw(out, self.time);
  serializeRaw(out, self.threadId);
  serializeRaw(out, self.name);
}

template <>
std::optional<ThreadNameEvent<false>>
EventHeader<EventType::ThreadName, ThreadNameEvent<false>, false>::deserialize(
    std::istream &data) {
  ThreadNameEvent<false> event;
  DESERIALIZE_RAW(event.time);
  DESERIALIZE_RAW(event.threadId);
  DESERIALIZE_RAW(event.name);
  return event;
}

void Event<true>::serialize(std::vector<std::byte> &out) const {
  serializeRaw(out, type());
  std::visit([&out](auto const &event) { event.serialize(out); }, event);
}

std::optional<Event<false>> Event<false>::deserialize(std::istream &data) {
  EventType type;
  DESERIALIZE_RAW(type);

  auto handleEvent = [&data]<class EventType>() -> std::optional<Event<false>> {
    auto eventOpt = EventType::deserialize(data);
    if (eventOpt.has_value()) {
      return Event(*eventOpt);
    }
    return std::nullopt;
  };

  switch (type) {
  case EventType::Start:
    return handleEvent.template operator()<StartEvent<false>>();
  case EventType::StartZone:
    return handleEvent.template operator()<StartZoneEvent<false>>();
  case EventType::EndZone:
    return handleEvent.template operator()<EndZoneEvent<false>>();
  case EventType::Message:
    return handleEvent.template operator()<MessageEvent<false>>();
  case EventType::ThreadName:
    return handleEvent.template operator()<ThreadNameEvent<false>>();
  case EventType::None:
    break;
  }
  return std::nullopt;
}
} // namespace TracyRecorder