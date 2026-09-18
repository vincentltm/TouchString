#pragma once

#include <array>
#include <random>
#include "nocopy.h"
#include "config.h"

namespace synthux {

class Scale {
public:
  Scale();
  ~Scale() {}

  uint8_t ScalesCount() {
    return kStringScalesCount;
  }

  void SetScaleIndex(uint8_t index) {
    if (index != _scale_index) {
      _scale_index = index;
      _PrepareScale();
    }
  }

  static constexpr std::array<uint8_t, 7> kVoiceToScaleIndex = { 0, 1, 2, 3, 4, 5, 7 };

  float TransMult(const float value) {
    auto new_trans_index = static_cast<uint8_t>(value * (_trans.size() - 1) + 0.5f);
    if (new_trans_index >= _trans.size()) new_trans_index = _trans.size() - 1;
    return _trans[new_trans_index];
  }

  float FreqAt(uint8_t idx) {
    uint8_t scale_idx = (idx < kVoiceToScaleIndex.size()) ? kVoiceToScaleIndex[idx] : idx;
    if (scale_idx >= kStringScaleSize) scale_idx = kStringScaleSize - 1;
    return _scale[scale_idx];
  }

  float Random() {
    std::uniform_int_distribution<uint8_t> note_distribution(0, 6);
    return FreqAt(note_distribution(_rand_engine));
  }

private:
  NOCOPY(Scale)

  void _PrepareScale() {
    auto transposition = _trans[_trans_index];
    auto& scale = kStringScales[_scale_index];
    for (uint8_t i = 0; i < kStringScaleSize; i++) {
      _scale[i] = scale[i] * transposition;
    }
  }

  uint8_t _scale_index;
  uint8_t _trans_index;
  std::default_random_engine _rand_engine;

  std::array<float, kStringScaleSize> _scale;

  static constexpr std::array<float, 49> _trans = {
    0.25000000f, 0.26486577f, 0.28061551f, 0.29730178f, 0.31498026f, 0.33370996f,
    0.35355339f, 0.37457677f, 0.39685026f, 0.42044821f, 0.44544936f, 0.47193716f,
    0.50000000f, 0.52973155f, 0.56123102f, 0.59460356f, 0.62996052f, 0.66741993f,
    0.70710678f, 0.74915354f, 0.79370053f, 0.84089642f, 0.89089872f, 0.94387431f,
    1.00000000f, // Center (0 semitones)
    1.05946309f, 1.12246205f, 1.18920712f, 1.25992105f, 1.33483985f, 1.41421356f,
    1.49830708f, 1.58740105f, 1.68179283f, 1.78179744f, 1.88774863f, 2.00000000f,
    2.11892619f, 2.24492410f, 2.37841423f, 2.51984210f, 2.66967971f, 2.82842712f,
    2.99661415f, 3.17480210f, 3.36358566f, 3.56359487f, 3.77549725f, 4.00000000f
  };
};

};
