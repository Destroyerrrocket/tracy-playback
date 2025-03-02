#include "playback.h"
#include <filesystem>
#include <fstream>
#include <iostream>

bool isPlaybackFile(std::ifstream &file) {
  std::string_view header = "TRCYPLAY\1\0\0\0";
  char magic[13] = {0};
  file.read(magic, 12);
  return file && std::string(magic, 12) == header;
}

int main(int argc, char **argv) {
  TracyPlayback::Playback playback;

  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <trace file/dir>" << std::endl;
    return 1;
  }

  auto addFile = [&](std::filesystem::path const &path) {
    auto file = std::make_unique<std::ifstream>(path, std::ios::binary);
    if (!*file) {
      std::cerr << "Failed to open file: " << path << std::endl;
      return 1;
    }

    if (!isPlaybackFile(*file)) { // Skip non-playback files
      return 0;
    }

    std::cout << "Adding trace file: " << path << std::endl;
    playback.addStream(
        TracyPlayback::Playback::StreamInfo{std::move(file), path.string()});
    return 0;
  };

  for (int i = 1; i < argc; ++i) {
    std::string traceFile = argv[i];
    if (std::filesystem::is_directory(traceFile)) {
      for (auto &entry : std::filesystem::directory_iterator(traceFile)) {
        if (entry.is_regular_file()) {
          if (auto errCode = addFile(entry.path())) {
            return errCode;
          }
        }
      }
      continue;
    } else {
      if (auto errCode = addFile(std::filesystem::path(traceFile))) {
        return errCode;
      }
    }
  }

  playback.play(true);
}