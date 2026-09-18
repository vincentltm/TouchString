#include "string_ui.h"
#include "daisysp.h"
#include "config.h"

using namespace synthux;
using namespace daisy;
using namespace daisysp;

float norm(const uint8_t value) {
    return static_cast<float>(value) / 127.f;
};

void StringUI::Init(daisy::DaisySeed& hw) {
    // Callbacks ................................................
    using namespace std::placeholders;
    auto on_touch = std::bind(&StringUI::_on_pad_touch, this, _1);
    auto on_release = std::bind(&StringUI::_on_pad_release, this, _1);
    _touch.pads().SetOnTouch(on_touch);
    _touch.pads().SetOnRelease(on_release);

    _pattern_value.Set(1.f);
    _shift_value.Set(0.f);
    _drive_value.Set(0.f);
    _in_vol_value.Set(0.f);

    _octave_shift = 0;
    _update_octave();

    // Initialize MIDI //////////////////////////////////////////
    /////////////////////////////////////////////////////////////
    #ifdef USB_MIDI
    daisy::MidiUsbHandler::Config midi_cfg;
    _midi.Init(midi_cfg);
    #endif
};

void StringUI::Process(DaisySeed& hw) {
    #ifdef USB_MIDI
    _process_midi();
    #endif

    // Process touch ////////////////////////////////////////////
    /////////////////////////////////////////////////////////////
    _touch.Process();
    _is_to_touched = _touch.pads().IsTouched(10);
    _is_ch_touched = _touch.pads().IsTouched(11);

    if (_is_first_run) {
        _is_first_run = false;
        auto initial_s36 = _touch.knobs().s36().Process();
        _drive_value.SetTracking(initial_s36);
        _string.SetDrive(initial_s36);

        auto initial_s33 = _touch.knobs().s33().Process();
        _pattern_value.SetTracking(initial_s33);
        _string.SetPattern(initial_s33);

        auto initial_s35 = _touch.knobs().s35().Process();
        _human_string_value.SetTracking(initial_s35);
        _string.SetHumanStringChance(initial_s35);
    }

    // Arpeggiator (Switch 2 / Right switch) .....................
    auto switch_value = _touch.switches().A();

    // Exciter mode (Switch 1 / Left switch) .....................
    auto exciter_switch = _touch.switches().B();
    if (exciter_switch == daisy::Switch3::POS_UP) {
        _exciter_mode = 0; // Pluck
    } else if (exciter_switch == daisy::Switch3::POS_DOWN) {
        _exciter_mode = 2; // Bow
    } else {
        _exciter_mode = 1; // Pluck + Bow on hold
    }
    _string.SetExciterMode(_exciter_mode);

    // Arpeggiator (Switch 2 / Right switch) .....................
    bool is_arp_on = switch_value != daisy::Switch3::POS_DOWN;
    bool arp_just_enabled = is_arp_on && !_was_arp_on;
    _was_arp_on = is_arp_on;

    _string.SetLatch(switch_value == daisy::Switch3::POS_UP);
    _string.SetArpOn(is_arp_on);

    if (arp_just_enabled) {
        for (uint8_t i = 0; i < 7; i++) {
            uint16_t p_idx = i + kFirstNotePad;
            if (_touch.pads().IsTouched(p_idx)) {
                _string.NoteOn(i, _touch.pads().Velocity(p_idx));
            }
        }
    }

    // String Dampen Pad (P01)
    bool p01_touched = _touch.pads().IsTouched(1);
    if (p01_touched && !_is_ch_touched) {
        float p01_press = _touch.pads().Pressure(1);
        _string.SetDampenPad(true, p01_press);
    } else {
        _string.SetDampenPad(false, 0.0f);
    }

    // Continuous pressure handling for bowing / hold ...........
    if (is_arp_on) {
        for (uint8_t i = 0; i < 7; i++) {
            uint16_t p_idx = i + kFirstNotePad;
            _string.SetPadPressure(i, _touch.pads().IsTouched(p_idx) ? _touch.pads().Pressure(p_idx) : 0.0f);
        }
    } else if (_string.IsMono()) {
        int8_t active = _string.ActiveMonoPad();
        if (active >= 0) {
            uint16_t p_idx = active + kFirstNotePad;
            float press = _touch.pads().IsTouched(p_idx) ? _touch.pads().Pressure(p_idx) : 0.0f;
            _string.SetPadPressure(active, press);

            if (_exciter_mode == 2) {
                // Bow mode: hand pressure directly modulates bowing on single centered voice
                _string.SetVoicePressure(0, press);
            } else if (_exciter_mode == 1) {
                // Pluck + Bow on hold
                _hold_ticks[active]++;
                if (_hold_ticks[active] > 6) {
                    _string.SetVoicePressure(0, press * 0.65f);
                    _string.SetVoiceSustain(0, true);
                } else {
                    _string.SetVoicePressure(0, 0.0f);
                    _string.SetVoiceSustain(0, false);
                }
            } else {
                _string.SetVoicePressure(0, 0.0f);
                _string.SetVoiceSustain(0, false);
                _string.SetVoiceAftertouch(0, press);
            }
        } else {
            _string.SetVoicePressure(0, 0.0f);
            _string.SetVoiceSustain(0, false);
            _string.SetVoiceAftertouch(0, 0.0f);
            for (uint8_t i = 0; i < 7; i++) {
                _hold_ticks[i] = 0;
            }
        }
    } else {
        if (_exciter_mode == 2) {
            for (uint8_t i = 0; i < 7; i++) {
                uint16_t p_idx = i + kFirstNotePad;
                if (_touch.pads().IsTouched(p_idx)) {
                    _string.SetVoicePressure(i, _touch.pads().Pressure(p_idx));
                } else {
                    _string.SetVoicePressure(i, 0.0f);
                }
            }
        } else if (_exciter_mode == 1) {
            for (uint8_t i = 0; i < 7; i++) {
                uint16_t p_idx = i + kFirstNotePad;
                if (_touch.pads().IsTouched(p_idx)) {
                    _hold_ticks[i]++;
                    if (_hold_ticks[i] > 6) {
                        // Warm bowed sustain cushion behind the pluck (matches switch down but slightly quieter)
                        _string.SetVoicePressure(i, _touch.pads().Pressure(p_idx) * 0.65f);
                        _string.SetVoiceSustain(i, true);
                    } else {
                        _string.SetPadPressure(i, _touch.pads().Pressure(p_idx));
                    }
                } else {
                    _hold_ticks[i] = 0;
                    _string.SetVoiceSustain(i, false);
                    _string.SetVoicePressure(i, 0.0f);
                    _string.SetPadPressure(i, 0.0f);
                }
            }
        } else {
            _string.SetSustain(false);
            for (uint8_t i = 0; i < 7; i++) {
                uint16_t p_idx = i + kFirstNotePad;
                float p = _touch.pads().IsTouched(p_idx) ? _touch.pads().Pressure(p_idx) : 0.0f;
                _string.SetPadPressure(i, p);
                _string.SetVoicePressure(i, 0.0f);
            }
        }
    }

    // Pitch (real-time) ..........................................
    _string.SetTransp(_touch.knobs().s31().Process());

    // Timbre (sample-and-hold at pluck time) .....................
    _string.SetDamping(_touch.knobs().s37().Process());
    _string.SetStructure(_touch.knobs().s32().Process());
    _string.SetBrightness(_touch.knobs().s30().Process());

    // Note randomization (arp only) ..............................
    _string.SetHumanNoteChance(_touch.knobs().s34().Process());

    // Human string chance <-> reverb mix .........................
    auto human_verb_knob_value = _touch.knobs().s35().Process();
    auto verb_stage = _verb_value.Process(human_verb_knob_value, _is_to_touched);
    auto human_string_raw = _human_string_value.Process(human_verb_knob_value, !_is_to_touched);
    _string.SetHumanStringChance(human_string_raw);
    _string.SetReverbMix(verb_stage);

    // Pattern density <-> pattern shift ..........................
    auto pattern_shift_value = _touch.knobs().s33().Process();
    auto pattern_amt = _pattern_value.Process(pattern_shift_value, !_is_to_touched);
    auto shift_amt = _shift_value.Process(pattern_shift_value, _is_to_touched);
    _string.SetPattern(pattern_amt);
    _string.SetPatternShift(shift_amt);

    // Drive / volume compensation <-> External Input Volume (shifted with TO) ....
    auto drive_fader_value = _touch.knobs().s36().Process();
    auto drive_amt = _drive_value.Process(drive_fader_value, !_is_to_touched);
    auto in_vol_amt = _in_vol_value.Process(drive_fader_value, _is_to_touched);
    _string.SetDrive(drive_amt);
    _string.SetInputVolume(in_vol_amt);

    if (_led_blink_counter > 0) {
        _led_blink_counter--;
        bool led_on = false;
        if (_led_blink_pattern == 1) {
            // Mono: Double blink
            if ((_led_blink_counter > 55) || (_led_blink_counter > 25 && _led_blink_counter <= 40)) {
                led_on = true;
            }
        } else if (_led_blink_pattern == 2) {
            // Poly / Reset: Single long blink
            if (_led_blink_counter > 15) {
                led_on = true;
            }
        } else if (_led_blink_pattern == 3) {
            // Octave step: Quick flash
            if (_led_blink_counter > 5) {
                led_on = true;
            }
        }
        hw.SetLed(led_on);
    } else {
        hw.SetLed(_string.IsLatched());
    }
};

void StringUI::_on_pad_touch(uint16_t pad) {
    // Scale, Tempo & Octave
    if (pad == 0) {
        if (_is_to_touched) {
            _string.SlowDown();
        } else if (_is_ch_touched) {
            _prev_scale();
        } else {
            if (_touch.pads().IsTouched(2)) {
                _octave_reset();
            } else {
                _octave_down();
            }
        }
        return;
    }
    if (pad == 2) {
        if (_is_to_touched) {
            _string.SpeedUp();
        } else if (_is_ch_touched) {
            _next_scale();
        } else {
            if (_touch.pads().IsTouched(0)) {
                _octave_reset();
            } else {
                _octave_up();
            }
        }
        return;
    }
    if (pad == 1) {
        if (_is_ch_touched) {
            _string.ToggleMono();
            _led_blink_pattern = _string.IsMono() ? 1 : 2;
            _led_blink_counter = 75;
        }
        return;
    }

    if (pad < kFirstNotePad || pad >= kFirstNotePad + String::kVoicesCount) return;
    auto note_num = pad - kFirstNotePad;
    float vel = _touch.pads().Velocity(pad);
    _hold_ticks[note_num] = 0;
    _string.NoteOn(note_num, vel);
};

void StringUI::_on_pad_release(uint16_t pad) {
    if (pad == 1) {
        _string.SetDampenPad(false, 0.0f);
        return;
    }
    if (pad < kFirstNotePad || pad >= kFirstNotePad + String::kVoicesCount) return;
    auto note_num = pad - kFirstNotePad;
    _hold_ticks[note_num] = 0;
    _string.NoteOff(note_num);
};

#ifdef USB_MIDI
void StringUI::_process_midi() {
    _string.ProcessClockIn(false);
    _midi.Listen();
    while(_midi.HasEvents()) {
        auto msg = _midi.PopEvent();
        switch(msg.type) {
            case SystemRealTime: {
                switch (msg.srt_type) {
                    case TimingClock: {
                        _string.ProcessClockIn(true);
                    }
                    break;
                    default: break;
                }
            }
            break;
            // TODO: unlike Bass, the string engine's NoteOn/NoteOff take a
            // scale-degree index (0..7), not a chromatic MIDI note number,
            // so incoming NoteOn/NoteOff messages aren't forwarded here.
            case ControlChange: {
                auto ctrl_msg = msg.AsControlChange();
                auto num = ctrl_msg.control_number;
                auto norm_value = norm(ctrl_msg.value);
                if (num == 70) { _string.SetBrightness(norm_value); }         // brightness
                else if (num == 71) { _string.SetTransp(norm_value); }       // transpose
                else if (num == 72) { _string.SetStructure(norm_value); }    // structure
                else if (num == 73) { _pattern_value.Set(norm_value); _string.SetPattern(norm_value); }        // pattern
                else if (num == 74) { _string.SetHumanNoteChance(norm_value); } // note randomization
                else if (num == 75) { _human_string_value.Set(norm_value); _string.SetHumanStringChance(norm_value); } // string randomization
                else if (num == 76) { _string.SetDamping(norm_value); }      // damping
                else if (num == 77) { _verb_value.Set(norm_value); _string.SetReverbMix(norm_value); }          // reverb
                else if (num == 78) { _shift_value.Set(norm_value); _string.SetPatternShift(norm_value); }      // pattern shift
                else if (num == 79) { _string.SetDrive(norm_value); }        // drive
                else if (num == 123) { _string.Reset(); }
            }
            break;

            default: break;
        }
    }
};
#endif
