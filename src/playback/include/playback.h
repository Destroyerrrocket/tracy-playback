#pragma once

#include <filesystem>
#include <memory>

class Playback {
public:
  Playback();
  ~Playback();
  void addFile(const std::filesystem::path &file);
  void play();

private:
  struct P;
  std::unique_ptr<P> p;
};