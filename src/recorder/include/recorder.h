#pragma once

#include <cstdint>
#include <functional>
#include <string_view>
#include <vector>

namespace TracyRecorder {
void setFlushCallback(
    const std::function<void(std::vector<std::byte> const &)> &output);

void flush();

void nameThread(std::string_view name);

void zoneStart(uint32_t line, std::string_view file, std::string_view function,
               std::string_view name, uint32_t color);
void zoneEnd();

void message(std::string_view message);
} // namespace TracyRecorder