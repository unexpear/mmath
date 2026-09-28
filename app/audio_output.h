#pragma once
#include "engine.h"

namespace app {
// Monitor gain is separate from DSP/analysis, and never overridden by a view.
struct AudioOutput {
  float volume = 1.0f;
  bool muted = false;
  bool apply(SDL_AudioStream* stream) const {
    return SDL_SetAudioStreamGain(stream, muted ? 0.0f : std::clamp(volume, 0.0f, 1.0f));
  }
};

// Caller has stopped the microphone. Restore the loaded file's actual rate and
// callback mode without discarding its reader position or A/B loop selection.
inline bool restoreFileSource(Engine& engine, SDL_AudioStream* stream, double fileRate) {
  if (!stream || !engine.reader || engine.trackFrames == 0) return false;
  SDL_LockAudioStream(stream);
  engine.mode.store(kNone);
  engine.playing.store(false);
  const SDL_AudioSpec spec{SDL_AUDIO_F32, 2, static_cast<int>(fileRate)};
  const bool ok = engine.buildProcessors(fileRate) &&
                  SDL_SetAudioStreamFormat(stream, &spec, nullptr) && SDL_ClearAudioStream(stream);
  engine.playRamp.setTarget(0.0, 0);
  engine.mode.store(ok ? kFile : kNone);
  SDL_UnlockAudioStream(stream);
  return ok;
}
}
