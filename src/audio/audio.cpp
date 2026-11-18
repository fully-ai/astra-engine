#include "astra/audio.hpp"

#include <SDL2/SDL.h>
#include <algorithm>
#include <cstring>

namespace astra {

bool AudioEngine::init() {
  if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
    ready_ = false;
    return false;
  }
  SDL_AudioSpec want{}, have{};
  want.freq = 22050;
  want.format = AUDIO_S16LSB;
  want.channels = 1;
  want.samples = 512;
  want.callback = AudioEngine::audio_callback;
  want.userdata = this;

  dev_id_ = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
  if (dev_id_ == 0) {
    ready_ = false;
    return false;
  }
  SDL_PauseAudioDevice(dev_id_, 0);
  ready_ = true;
  return true;
}

void AudioEngine::shutdown() {
  if (dev_id_ != 0) {
    SDL_CloseAudioDevice(dev_id_);
    dev_id_ = 0;
  }
  ready_ = false;
}

void AudioEngine::play(const WavData& wav) {
  if (!ready_ || wav.samples.empty()) return;
  std::lock_guard<std::mutex> lock(mu_);
  if (read_pos_ > 0) {
    mix_buf_.erase(mix_buf_.begin(), mix_buf_.begin() + static_cast<ptrdiff_t>(read_pos_));
    read_pos_ = 0;
  }
  mix_buf_.insert(mix_buf_.end(), wav.samples.begin(), wav.samples.end());
}

void AudioEngine::audio_callback(void* userdata, uint8_t* stream, int len) {
  auto* self = static_cast<AudioEngine*>(userdata);
  std::memset(stream, 0, static_cast<size_t>(len));
  std::lock_guard<std::mutex> lock(self->mu_);
  size_t need = static_cast<size_t>(len) / sizeof(int16_t);
  size_t avail = self->mix_buf_.size() - self->read_pos_;
  size_t copy_n = std::min(need, avail);
  if (copy_n > 0) {
    std::memcpy(stream, self->mix_buf_.data() + self->read_pos_, copy_n * sizeof(int16_t));
    self->read_pos_ += copy_n;
  }
}

}  // namespace astra
