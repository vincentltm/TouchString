#include "string.h"
#include <functional>

using namespace synthux;

String::String():
_trigger              { Trigger(kPPQN) },
_dice                 { std::uniform_int_distribution<uint8_t>(0, 100) },
_tempo                { .45f },
_brightness           { 0.f },
_structure            { 0.f },
_damping              { 0.f },
_human_note_chance    { 0 },
_human_string_chance  { 0 },
_volume               { 1.f },
_is_arp_on            { false },
_exciter_mode         { 0 },
_input_volume         { 0.f },
_trans_mult           { 1.f },
_is_mono              { false },
_active_mono_voice    { 0xff },
_mono_stack_size      { 0 }
{
  _mono_stack.fill(0);
  _note_on.fill(false);
  _note_hold.fill(false);
  _pad_pressure.fill(0.f);
  _reverb_in.fill(0.f);
  _reverb_out.fill(0.f);
  _bus.fill(0.f);
};

void String::SetMono(const bool mono) {
  if (_is_mono != mono) {
    _is_mono = mono;
    _mono_stack_size = 0;
    _active_mono_voice = 0xff;
    for (auto& v : _vox) {
      v.SetBowPressure(0.0f);
      v.SetSustain(false);
    }
  }
}

void String::Init(const float sample_rate, const float buffer_size) {
  using namespace std::placeholders;

  _clock.Init(1e6 * buffer_size / sample_rate, kPPQNExtern, kPPQN);
  auto on_clock = std::bind(&String::_on_clock_tick, this);
  _clock.SetOnTick(on_clock);

  auto on_latch_note_on = std::bind(&String::_on_latch_note_on, this, _1);
  auto on_latch_note_off = std::bind(&String::_on_latch_note_off, this, _1);
  _latch.set_on_note_on(on_latch_note_on);
  _latch.set_on_note_off(on_latch_note_off);

  auto on_arp_note_on = std::bind(&String::_on_arp_note_on, this, _1, _2);
  auto on_arp_note_off = std::bind(&String::_on_arp_note_off, this, _1);
  _arp.SetOnNoteOn(on_arp_note_on);
  _arp.SetOnNoteOff(on_arp_note_off);
  
  _arp.SetDirection(ArpDirection::fwd);
  _arp.SetRandChance(0);
  _arp.SetAsPlayed(true);
  _arp.SetNonLegato(true);

  for (size_t i = 0; i < kVoicesCount; i++) {
    _vox[i].Init(sample_rate, _scale.FreqAt(i), 123456789u + static_cast<uint32_t>(i) * 987654321u);
  }

  _drive.Init();

  _reverb.Init(sample_rate);
  _reverb.SetFeedback(kReverbFeedback);
  _reverb.SetLpFreq(kReverLPFreq);

  SetTempo(_tempo);
};

void String::SetLatch(const bool on) {
    _latch.set_on(on);
    if (_is_arp_on && !_arp.HasNote()) Reset();
};

void String::SetScaleIndex(const uint8_t index) {
  _scale.SetScaleIndex(index);
  for (size_t i = 0; i < kVoicesCount; i++) {
    _vox[i].SetFreq(_scale.FreqAt(i));
  }
}

void String::SetTransp(const float value) {
  _trans_mult = _scale.TransMult(value);
  bool is_realtime = (_exciter_mode == 2) || (_input_volume > 0.005f);
  for (size_t i = 0; i < kVoicesCount; i++) {
    if (is_realtime || _vox[i].IsBowing() || !_vox[i].IsActive()) {
      _vox[i].SetMult(_trans_mult);
    }
  }
}

void String::SetBrightness(const float value) {
  _brightness = value;
  for (auto& v : _vox) {
    v.SetBrightness(value);
  }
}

void String::SetStructure(const float value) {
  _structure = value;
  for (auto& v : _vox) {
    v.SetStructure(value);
  }
}

void String::SetDamping(const float value) {
  _damping = value * .7f;
  for (auto& v : _vox) {
    v.SetDamping(_damping);
  }
}

void String::SetVoicePressure(const uint8_t voice_num, const float pressure) {
  if (voice_num < kVoicesCount) {
    _vox[voice_num].SetBowPressure(pressure);
  }
}

void String::SetVoiceSustain(const uint8_t voice_num, const bool sustain) {
  if (voice_num < kVoicesCount) {
    _vox[voice_num].SetSustain(sustain);
  }
}

void String::SetBowPressure(const float pressure) {
  for (auto& v : _vox) {
    v.SetBowPressure(pressure);
  }
}

void String::SetSustain(const bool sustain) {
  for (auto& v : _vox) {
    v.SetSustain(sustain);
  }
}

void String::SetNoteFreq(const uint8_t note_num) {
  if (note_num < kVoicesCount) {
    _vox[note_num].SetFreq(_scale.FreqAt(note_num));
  }
}

void String::NoteOn(const uint8_t num, const float velocity) {
  if (num < kVoicesCount) {
    _vox[num].SetMult(_trans_mult, false);
    _pad_pressure[num] = velocity;
    _humanize_and_apply(num);
    if (!_is_arp_on) {
      if (_is_mono) {
        // Manage mono note stack
        bool already_in = false;
        for (uint8_t i = 0; i < _mono_stack_size; i++) {
          if (_mono_stack[i] == num) {
            for (uint8_t j = i; j + 1 < _mono_stack_size; j++) {
              _mono_stack[j] = _mono_stack[j + 1];
            }
            _mono_stack[_mono_stack_size - 1] = num;
            already_in = true;
            break;
          }
        }
        if (!already_in && _mono_stack_size < kVoicesCount) {
          _mono_stack[_mono_stack_size++] = num;
        }

        if (_exciter_mode == 2) {
          // Bow mode: legato transition
          if (_active_mono_voice < kVoicesCount && _vox[_active_mono_voice].IsBowing()) {
            _vox[_active_mono_voice].SetFreq(_scale.FreqAt(num));
            _vox[_active_mono_voice].SetBowPressure(velocity);
          } else {
            for (auto& v : _vox) {
              v.SetBowPressure(0.0f);
              v.SetSustain(false);
            }
            _active_mono_voice = num;
            _vox[num].SetFreq(_scale.FreqAt(num));
            _vox[num].SetBowPressure(velocity);
          }
        } else {
          // Pluck / Pluck+Bow mode: silence other voices and strike new note
          for (size_t i = 0; i < kVoicesCount; i++) {
            if (i != num) {
              _vox[i].Reset();
            }
          }
          _active_mono_voice = num;
          _vox[num].SetSustain(false);
          _vox[num].SetBowPressure(0.0f);
          _vox[num].NoteOn(_scale.FreqAt(num), velocity);
        }
        return;
      } else {
        // Poly mode
        if (_exciter_mode == 2) {
          _vox[num].SetFreq(_scale.FreqAt(num));
          _vox[num].SetBowPressure(velocity);
        } else {
          _vox[num].SetSustain(false);
          _vox[num].SetBowPressure(0.0f);
          _vox[num].NoteOn(_scale.FreqAt(num), velocity);
        }
        return;
      }
    }
  }

  _latch.note_on(num);

  if (_arp.HasNote()) {
    if (!_clock.IsRunning()) _clock.Run();
  }
  else {
    Reset();
  }
};

void String::NoteOff(const uint8_t num) {
  if (num < kVoicesCount) {
    _pad_pressure[num] = 0.0f;
    _vox[num].SetAftertouch(0.0f);
    if (!_is_arp_on) {
      if (_is_mono) {
        // Remove num from mono stack
        for (uint8_t i = 0; i < _mono_stack_size; i++) {
          if (_mono_stack[i] == num) {
            for (uint8_t j = i; j + 1 < _mono_stack_size; j++) {
              _mono_stack[j] = _mono_stack[j + 1];
            }
            _mono_stack_size--;
            break;
          }
        }

        if (_mono_stack_size > 0) {
          // Legato return to top of stack
          uint8_t prev_num = _mono_stack[_mono_stack_size - 1];
          float prev_press = _pad_pressure[prev_num];
          if (prev_press < 0.2f) prev_press = 0.6f;

          if (_exciter_mode == 2) {
            if (_active_mono_voice < kVoicesCount) {
              _vox[_active_mono_voice].SetFreq(_scale.FreqAt(prev_num));
              _vox[_active_mono_voice].SetBowPressure(prev_press);
            } else {
              _active_mono_voice = prev_num;
              _vox[prev_num].SetFreq(_scale.FreqAt(prev_num));
              _vox[prev_num].SetBowPressure(prev_press);
            }
          } else {
            for (size_t i = 0; i < kVoicesCount; i++) {
              if (i != prev_num) _vox[i].Reset();
            }
            _active_mono_voice = prev_num;
            _vox[prev_num].NoteOn(_scale.FreqAt(prev_num), prev_press);
          }
        } else {
          // Stack empty - silence
          if (_active_mono_voice < kVoicesCount) {
            _vox[_active_mono_voice].SetSustain(false);
            _vox[_active_mono_voice].SetBowPressure(0.0f);
          }
          _active_mono_voice = 0xff;
        }
        return;
      } else {
        // Poly mode
        _vox[num].SetSustain(false);
        _vox[num].SetBowPressure(0.0f);
      }
    }
  }

  if (_is_arp_on) {
    _latch.note_off(num);
    if (!_arp.HasNote()) {
      Reset();
    }
  }
};

void String::Reset() {
  _clock.Stop();
  _trigger.Reset();
  _pattern.Reset();
  _arp.Clear();
  _latch.clear();
  _mono_stack_size = 0;
  _active_mono_voice = 0xff;
  for (auto& v : _vox) {
    v.SetSustain(false);
    v.SetBowPressure(0.0f);
    v.SetAftertouch(0.0f);
  }
};

void String::Process(const float * const *in, float **out, size_t size) {
  _clock.Tick();
  for (size_t i = 0; i < size; i++) {
    float ext_audio = 0.f;
    if (in != nullptr && _input_volume > 0.005f) {
      ext_audio = (in[0][i] + in[1][i]) * 0.5f * _input_volume;
      ext_audio = daisysp::SoftLimit(ext_audio * 1.5f);
    }

    float sum_l = 0.f;
    float sum_r = 0.f;
    for (size_t v = 0; v < kVoicesCount; v++) {
      float s = _vox[v].Process(ext_audio * 0.40f);
      sum_l += s * kPanL[v];
      sum_r += s * kPanR[v];
    }
    sum_l *= 0.65f;
    sum_r *= 0.65f;
    _bus[0] = _drive.Process(sum_l) * _volume;
    _bus[1] = _drive.Process(sum_r) * _volume;
    _xfade.Process(0, 0, _bus[0], _bus[1], _reverb_in[0], _reverb_in[1]);
    _reverb.Process(_reverb_in[0], _reverb_in[1], &(_reverb_out[0]), &(_reverb_out[1]));
    out[0][i] = daisysp::SoftLimit(_bus[0] + _reverb_out[0]);
    out[1][i] = daisysp::SoftLimit(_bus[1] + _reverb_out[1]);
  }
};

void String::_on_clock_tick() {
  if (_trigger.Tick() && _pattern.Tick()) {
    _arp.Trigger();
  }
};

void String::_on_latch_note_on(uint8_t num) 
{ 
  _arp.NoteOn(num, 127); 
}
void String::_on_latch_note_off(uint8_t num) 
{ 
  _arp.NoteOff(num); 
}

void String::_on_arp_note_on(uint8_t num, uint8_t vel) {
  if (num >= kVoicesCount) return;
  if (_is_mono) {
    for (size_t i = 0; i < kVoicesCount; i++) {
      if (i != num) {
        _vox[i].Reset();
      }
    }
  }
  _vox[num].SetMult(_trans_mult, false);
  auto freq = _is_arp_on ? _humanized_note_freq(num) : _scale.FreqAt(num);
  _humanize_and_apply(num);
  float p = _pad_pressure[num];
  float pluck_vel = (p > 0.05f) ? daisysp::fclamp(sqrtf(p), 0.25f, 1.0f) : 0.85f;

  if (_exciter_mode == 2) {
    // Bow mode: soft strike transient + rich bowing excitation
    _vox[num].NoteOn(freq, pluck_vel * 0.40f);
    _vox[num].SetBowPressure(pluck_vel);
    _vox[num].SetSustain(true);
  } else if (_exciter_mode == 1) {
    // Pluck + Bow mode: full pluck strike + warm bowed sustain cushion
    _vox[num].NoteOn(freq, pluck_vel);
    _vox[num].SetBowPressure(pluck_vel * 0.65f);
    _vox[num].SetSustain(true);
  } else {
    // Pure Pluck mode: crisp pluck strike, zero bow pressure
    _vox[num].SetSustain(false);
    _vox[num].SetBowPressure(0.0f);
    _vox[num].NoteOn(freq, pluck_vel);
  }
};

void String::_on_arp_note_off(uint8_t num) {
  if (num < kVoicesCount) {
    _vox[num].SetBowPressure(0.0f);
    _vox[num].SetSustain(false);
  }
}

float String::_humanized_note_freq(uint8_t note) {
  auto freq = _scale.FreqAt(note);
  if (_human_note_chance <= 2) return freq;

  auto human_note_chance_dice = _dice(_rand_engine);
  auto note_dice = _dice(_rand_engine);
  auto octave_dice = _dice(_rand_engine);

  if (_human_note_chance < 33) {
    if (human_note_chance_dice < _human_note_chance) {
      return (octave_dice < 50) ? freq * .5f : freq * 2.f;
    }
    return freq;
  }
  else if (_human_note_chance < 66) {
    if (human_note_chance_dice < _human_note_chance) {
      if (note_dice < 50) freq = _scale.Random();
      if (octave_dice < 25) return freq * .5f;
      else if (octave_dice > 75) return freq * 2.f;
      else return freq;
    }
    return freq;
  }
  else {
    if (note_dice < _human_note_chance) freq = _scale.Random();
    if (octave_dice < 20) return freq * 2.f;
    else if (octave_dice > 80) return freq * .5f;
    return freq; // 60% of freq passes through unchanged
  }
};

void String::_humanize_and_apply(uint8_t voice_num) {
  if (voice_num >= kVoicesCount) return;
  float b = _brightness;
  float s = _structure;
  float d = _damping;
  if (_human_string_chance > 2) {
    auto chance_dice = _dice(_rand_engine);
    if (chance_dice < _human_string_chance) {
      auto bright_dice = _dice(_rand_engine);
      auto structure_dice = _dice(_rand_engine);
      auto damping_dice = _dice(_rand_engine);

      auto bright_delta = bright_dice * 0.002f;
      b = std::min(b + bright_delta, 1.f);

      auto struct_delta = structure_dice * 0.002f;
      s = std::min(s + struct_delta, 1.f);

      auto damping_delta = damping_dice * 0.004f;
      d = std::min(d - 0.004f + damping_delta, 8.f);
    }
  }
  _vox[voice_num].SetBrightness(b);
  _vox[voice_num].SetStructure(s);
  _vox[voice_num].SetDamping(d);
};
