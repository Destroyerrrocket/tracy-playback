#pragma once

#include <cstdint>
#include <istream>
#include <optional>
#include <string_view>
#include <variant>
#include <vector>

namespace TracyRecorder {

template <bool isOut>
using OutInString = std::conditional_t<isOut, std::string_view, std::string>;

enum class EventType {
  None = -1,
  Start = 0,
  StartZone = 1,
  EndZone = 2,
  Message = 3,
  ThreadName = 4,
};

template <EventType eventType, class Event, bool isOut> struct EventHeader;
template <EventType eventType, class Event>
struct EventHeader<eventType, Event, true> {
  EventHeader() = default;
  EventHeader(EventHeader &&) = default;
  EventHeader(EventHeader const &) = default;
  EventHeader &operator=(EventHeader const &) = default;
  EventHeader &operator=(EventHeader &&) = default;

  bool operator==(EventHeader const &other) const = default;
  auto operator<=>(EventHeader const &other) const = default;

  constexpr static EventType type() { return eventType; }

  static void serialize(Event const &self, std::vector<std::byte> &out);

  void serialize(std::vector<std::byte> &out) const {
    serialize(static_cast<Event const &>(*this), out);
  }
};

template <EventType eventType, class Event>
struct EventHeader<eventType, Event, false> {
  EventHeader() = default;
  EventHeader(EventHeader &&) = default;
  EventHeader(EventHeader const &) = default;
  EventHeader &operator=(EventHeader const &) = default;
  EventHeader &operator=(EventHeader &&) = default;

  bool operator==(EventHeader const &other) const = default;
  auto operator<=>(EventHeader const &other) const = default;

  constexpr static EventType type() { return eventType; }

  static std::optional<Event> deserialize(std::istream &data);
};

template <EventType eventType, class Event, bool isOut>
struct ThreadEvent : public EventHeader<eventType, Event, isOut> {
  ThreadEvent() = default;
  ThreadEvent(uint64_t threadId, uint64_t time)
      : threadId{threadId}, time{time} {}
  ThreadEvent(ThreadEvent &&) = default;
  ThreadEvent(ThreadEvent const &) = default;
  ThreadEvent &operator=(ThreadEvent const &) = default;
  ThreadEvent &operator=(ThreadEvent &&) = default;

  bool operator==(ThreadEvent const &other) const = default;
  auto operator<=>(ThreadEvent const &other) const = default;

  uint64_t threadId;
  uint64_t time;
};

template <bool isOut>
struct StartEvent
    : public EventHeader<EventType::Start, StartEvent<isOut>, isOut> {
  StartEvent() = default;
  StartEvent(OutInString<isOut> host, uint64_t unixTime, uint64_t processId)
      : host{host}, unixTime{unixTime}, processId{processId} {}
  StartEvent(StartEvent &&) = default;
  StartEvent(StartEvent const &) = default;
  StartEvent &operator=(StartEvent const &) = default;
  StartEvent &operator=(StartEvent &&) = default;

  bool operator==(StartEvent const &other) const = default;
  auto operator<=>(StartEvent const &other) const = default;

  OutInString<isOut> host;
  uint64_t unixTime;
  uint64_t processId;
};

template <bool isOut>
struct StartZoneEvent
    : public ThreadEvent<EventType::StartZone, StartZoneEvent<isOut>, isOut> {
  StartZoneEvent() = default;
  StartZoneEvent(uint32_t color, uint32_t line, OutInString<isOut> file,
                 OutInString<isOut> function, OutInString<isOut> name,
                 uint64_t threadId, uint64_t time)
      : ThreadEvent<EventType::StartZone, StartZoneEvent<isOut>,
                    isOut>{threadId, time},
        color{color}, line{line}, file{file}, function{function}, name{name} {}
  StartZoneEvent(StartZoneEvent &&) = default;
  StartZoneEvent(StartZoneEvent const &) = default;
  StartZoneEvent &operator=(StartZoneEvent const &) = default;
  StartZoneEvent &operator=(StartZoneEvent &&) = default;

  bool operator==(StartZoneEvent const &other) const = default;
  auto operator<=>(StartZoneEvent const &other) const = default;

  uint32_t color;
  uint32_t line;
  OutInString<isOut> file;
  OutInString<isOut> function;
  OutInString<isOut> name;
};

template <bool isOut>
struct EndZoneEvent
    : public ThreadEvent<EventType::EndZone, EndZoneEvent<isOut>, isOut> {
  EndZoneEvent() = default;
  EndZoneEvent(uint64_t threadId, uint64_t time)
      : ThreadEvent<EventType::EndZone, EndZoneEvent<isOut>, isOut>(threadId,
                                                                    time) {}
  EndZoneEvent(EndZoneEvent &&) = default;
  EndZoneEvent(EndZoneEvent const &) = default;
  EndZoneEvent &operator=(EndZoneEvent const &) = default;
  EndZoneEvent &operator=(EndZoneEvent &&) = default;

  bool operator==(EndZoneEvent const &other) const = default;
  auto operator<=>(EndZoneEvent const &other) const = default;
};

template <bool isOut>
struct MessageEvent
    : public ThreadEvent<EventType::Message, MessageEvent<isOut>, isOut> {
  MessageEvent() = default;
  MessageEvent(OutInString<isOut> message, uint32_t color, uint64_t threadId,
               uint64_t time)
      : ThreadEvent<EventType::Message, MessageEvent<isOut>, isOut>(threadId,
                                                                    time),
        message{message}, color{color} {}
  MessageEvent(MessageEvent &&) = default;
  MessageEvent(MessageEvent const &) = default;
  MessageEvent &operator=(MessageEvent const &) = default;
  MessageEvent &operator=(MessageEvent &&) = default;

  bool operator==(MessageEvent const &other) const = default;
  auto operator<=>(MessageEvent const &other) const = default;

  OutInString<isOut> message;
  uint32_t color;
};

template <bool isOut>
struct ThreadNameEvent
    : public ThreadEvent<EventType::ThreadName, ThreadNameEvent<isOut>, isOut> {
  ThreadNameEvent() = default;
  ThreadNameEvent(OutInString<isOut> name, uint64_t threadId, uint64_t time)
      : ThreadEvent<EventType::ThreadName, ThreadNameEvent<isOut>, isOut>(
            threadId, time),
        name{name} {}
  ThreadNameEvent(ThreadNameEvent &&) = default;
  ThreadNameEvent(ThreadNameEvent const &) = default;
  ThreadNameEvent &operator=(ThreadNameEvent const &) = default;
  ThreadNameEvent &operator=(ThreadNameEvent &&) = default;

  bool operator==(ThreadNameEvent const &other) const = default;
  auto operator<=>(ThreadNameEvent const &other) const = default;

  OutInString<isOut> name;
};

template <bool isOut>
using AllEvents =
    std::variant<StartEvent<isOut>, StartZoneEvent<isOut>, EndZoneEvent<isOut>,
                 MessageEvent<isOut>, ThreadNameEvent<isOut>>;

template <bool isOut> struct EventCommon {
  AllEvents<isOut> event;

  EventCommon() = default;
  template <template <bool> class SpecificEvent>
  EventCommon(SpecificEvent<isOut> specificEvent) {
    event = specificEvent;
  }
  EventCommon(EventCommon &&) = default;
  EventCommon(EventCommon const &) = default;
  EventCommon &operator=(EventCommon const &) = default;
  EventCommon &operator=(EventCommon &&) = default;

  bool operator==(EventCommon const &other) const = default;
  auto operator<=>(EventCommon const &other) const = default;

  EventType type() const {
    return std::visit([](auto const &event) { return event.type(); }, event);
  }
};

template <bool isOut> struct Event;

template <> struct Event<true> : EventCommon<true> {
  using EventCommon<true>::EventCommon;
  Event() = default;
  Event(Event &&) = default;
  Event(Event const &) = default;
  Event &operator=(Event const &) = default;
  Event &operator=(Event &&) = default;

  bool operator==(Event const &other) const = default;
  auto operator<=>(Event const &other) const = default;

  void serialize(std::vector<std::byte> &out) const;
};

template <> struct Event<false> : EventCommon<false> {
  using EventCommon<false>::EventCommon;
  Event() = default;
  Event(Event &&) = default;
  Event(Event const &) = default;
  Event &operator=(Event const &) = default;
  Event &operator=(Event &&) = default;

  bool operator==(Event const &other) const = default;
  auto operator<=>(Event const &other) const = default;

  static std::optional<Event> deserialize(std::istream &data);
};

template <bool isOut, template <bool> class SpecificEvent>
Event(SpecificEvent<isOut>) -> Event<isOut>;

} // namespace TracyRecorder