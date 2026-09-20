#pragma once

#include "daisy_seed.h"
#include "../touch/touch.h"
#include "../string/string.h"
#include "config.h"
#include "mvalue.h"
#include <array>
#include <functional>

namespace synthux {

class StringUI {
public:
    StringUI(Touch& touch, String& string):
    _touch { touch },
    _string { string },
    _scale_index { 0 },
    _octave_shift { 0 },
    _exciter_mode { 0 },
    _was_arp_on { false },
    _is_first_run { true },
    _led_blink_counter { 0 },
    _led_blink_pattern { 0 }
     {
         _hold_ticks.fill(0);
     }

    ~StringUI() {}

    void Init(daisy::DaisySeed& hw);
    void Process(daisy::DaisySeed& hw);

private:
    void _next_scale() {
        if (_scale_index == static_cast<uint8_t>(_string.ScalesCount() - 1)) return;
        _scale_index ++;
        _string.SetScaleIndex(_scale_index);
    }

    void _prev_scale() {
        if (_scale_index == 0) return;
        _scale_index --;
        _string.SetScaleIndex(_scale_index);
    }

    void _octave_down() {
        if (_octave_shift > -2) {
            _octave_shift--;
            _update_octave();
            _led_blink_pattern = 3;
            _led_blink_counter = 25;
        }
    }

    void _octave_up() {
        if (_octave_shift < 2) {
            _octave_shift++;
            _update_octave();
            _led_blink_pattern = 3;
            _led_blink_counter = 25;
        }
    }

    void _octave_reset() {
        if (_octave_shift != 0) {
            _octave_shift = 0;
            _update_octave();
            _led_blink_pattern = 2;
            _led_blink_counter = 50;
        }
    }

    void _update_octave() {
        static constexpr float kOctaveMults[5] = { 0.25f, 0.50f, 1.00f, 2.00f, 4.00f };
        float mult = kOctaveMults[_octave_shift + 2];
        _string.SetCurrentOctaveMult(mult);
    }

    void _on_pad_touch(uint16_t pad);
    void _on_pad_release(uint16_t pad);

    #ifdef USB_MIDI
    daisy::MidiUsbHandler _midi;
    void _process_midi();
    #endif

    Touch& _touch;
    String& _string;

    MValue _verb_value;
    MValue _human_string_value;
    MValue _pattern_value;
    MValue _shift_value;
    MValue _drive_value;
    MValue _in_vol_value;

    static constexpr uint8_t kNotesCount = 8;
    static constexpr uint16_t kFirstNotePad = 3;

    uint8_t _scale_index;
    int8_t _octave_shift;
    bool _is_to_touched;
    bool _is_ch_touched;
    std::array<uint16_t, 7> _hold_ticks;
    int _exciter_mode;
    bool _was_arp_on;
    bool _is_first_run;
    uint8_t _led_blink_counter;
    uint8_t _led_blink_pattern;
};

};
