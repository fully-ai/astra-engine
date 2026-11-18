#pragma once

#include <mutex>
#include <vector>

#include "astra/assets.hpp"

namespace astra {

class AudioEngine {
 public:
  bool init();
  void shutdown();
  void play(const WavData& wav);
  bool hardware_ready() const { return ready_; }

 private:
  bool ready_ = false;
  uint32_t dev_id_ = 0;
  std::mutex mu_;
  std::vector<int16_t> mix_buf_;
  size_t read_pos_ = 0;
  static void audio_callback(void* userdata, uint8_t* stream, int len);
};

}  // namespace astra
