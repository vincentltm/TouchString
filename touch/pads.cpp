#include "pads.h"
#include "daisysp.h"
#include <cmath>

using namespace synthux;
using namespace daisy;

void Pads::WriteRegister(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = { reg, val };
    _i2c.TransmitBlocking(kMpr121Addr, buf, 2, 10);
}

bool Pads::ReadBurst(uint8_t start_reg, uint8_t* buffer, uint16_t size) {
    if (_i2c.TransmitBlocking(kMpr121Addr, &start_reg, 1, 10) != I2CHandle::Result::OK) {
        return false;
    }
    return (_i2c.ReceiveBlocking(kMpr121Addr, buffer, size, 10) == I2CHandle::Result::OK);
}

void Pads::Init(DaisySeed& hw) {
    I2CHandle::Config i2c_conf;
    i2c_conf.mode = I2CHandle::Config::Mode::I2C_MASTER;
    i2c_conf.periph = I2CHandle::Config::Peripheral::I2C_1;
    i2c_conf.speed = I2CHandle::Config::Speed::I2C_400KHZ;
    i2c_conf.pin_config.scl = Pin(PORTB, 8);
    i2c_conf.pin_config.sda = Pin(PORTB, 9);
    _i2c.Init(i2c_conf);

    // Soft reset
    WriteRegister(0x80, 0x63);
    System::Delay(5);

    // Stop mode to configure registers
    WriteRegister(0x5E, 0x00);

    // Touch and Release thresholds for all 12 electrodes
    // Touch threshold = 10 counts: sensitive soft-touch response
    // Release threshold = 6 counts: clean hysteresis, zero chatter, zero stuck pads
    for (uint8_t i = 0; i < 12; i++) {
        WriteRegister(0x41 + i * 2, 10); // ELEx Touch threshold
        WriteRegister(0x42 + i * 2, 6);  // ELEx Release threshold
    }

    // Filter configuration (Rising / Release): official NXP AN3891 & libDaisy
    WriteRegister(0x2B, 0x01); // MHDR: Max Half Delta Rising
    WriteRegister(0x2C, 0x01); // NHDR: Noise Half Delta Rising
    WriteRegister(0x2D, 0x14); // NCLR: Noise Count Limit Rising
    WriteRegister(0x2E, 0x80); // FDLR: Filter Delay Limit Rising

    // Filter configuration (Falling / Touch): official NXP AN3891 & libDaisy
    WriteRegister(0x2F, 0x01); // MHDF: Max Half Delta Falling
    WriteRegister(0x30, 0x05); // NHDF: Noise Half Delta Falling
    WriteRegister(0x31, 0x01); // NCLF: Noise Count Limit Falling
    WriteRegister(0x32, 0x80); // FDLF: Filter Delay Limit Falling

    // Touched filter configuration: baseline frozen during touch
    WriteRegister(0x33, 0x00); // NHD_T
    WriteRegister(0x34, 0x00); // NCL_T
    WriteRegister(0x35, 0x00); // FDL_T

    // Debounce configuration: register 0x5B
    // bits [6:4] = Release debounce (2 samples = 0b010), bits [2:0] = Touch debounce (1 sample = 0b001)
    WriteRegister(0x5B, 0x21);

    // Electrode charge current & charge time configuration
    WriteRegister(0x5C, 0x10); // CONFIG1: 16 uA charge current
    WriteRegister(0x5D, 0x20); // CONFIG2: 0.5 us encoding, 1 ms sample period

    // Run mode step 1: 12 electrodes enabled, initialize baseline from resting state
    // 0x8C: CL = 10 (init baseline from first samples), 12 electrodes
    WriteRegister(0x5E, 0x8C);
    System::Delay(80);

    // Run mode step 2: Lock baseline tracking completely (CL = 00)
    // Disables hardware baseline adaptation and auto-reconfiguration so touching all pads
    // simultaneously never re-baselines over touched fingers, eliminating stuck pads!
    WriteRegister(0x5E, 0x0C);
}

void Pads::Process() {
    uint8_t raw[42];
    if (!ReadBurst(0x00, raw, 42)) {
        return;
    }

    uint16_t raw_state = (static_cast<uint16_t>(raw[1] & 0x0F) << 8) | raw[0];

    for (uint16_t i = 0; i < 12; i++) {
        uint16_t mask = 1 << i;
        bool raw_touched = (raw_state & mask) != 0;
        bool was_touched = (_state & mask) != 0;
        bool state_changed = false;

        uint16_t filt = (static_cast<uint16_t>(raw[4 + i * 2 + 1] & 0x03) << 8) | raw[4 + i * 2];
        uint16_t base = static_cast<uint16_t>(raw[0x1E + i]) << 2;

        int32_t delta = static_cast<int32_t>(base) - static_cast<int32_t>(filt);
        if (delta < 0) delta = 0;

        if (_release_lockout[i] > 0) {
            _release_lockout[i]--;
        }

        if (raw_touched != was_touched) {
            if (raw_touched) {
                if (_release_lockout[i] == 0) {
                    _state |= mask;
                    state_changed = true;
                    _debounce_cnt[i] = 0;
                    _strike_window[i] = 4;
                    _strike_peak_delta[i] = delta;
                }
            } else {
                _debounce_cnt[i]++;
                if (_debounce_cnt[i] >= 2) {
                    _debounce_cnt[i] = 0;
                    _state &= ~mask;
                    state_changed = true;
                }
            }
        } else {
            _debounce_cnt[i] = 0;
        }

        bool is_touched = (_state & mask) != 0;

        if (is_touched) {
            float target_p = 0.0f;
            if (delta >= 6) {
                float effective_max = _pad_max_delta[i] - 5.0f;
                if (effective_max < 30.0f) effective_max = 30.0f;

                float norm = static_cast<float>(delta - 5) / effective_max;
                if (norm > 1.0f) norm = 1.0f;
                if (norm < 0.0f) norm = 0.0f;

                target_p = norm * norm;
            }

            if (_strike_window[i] > 0) {
                if (delta > _strike_peak_delta[i]) {
                    _strike_peak_delta[i] = delta;
                }
                _pressure[i] = target_p;
                _strike_window[i]--;

                bool peak_passed = (_strike_peak_delta[i] > 30 && delta < (_strike_peak_delta[i] - 12));
                if (_strike_window[i] == 0 || peak_passed) {
                    _strike_window[i] = 0;

                    float effective_max = _pad_max_delta[i] - 5.0f;
                    if (effective_max < 30.0f) effective_max = 30.0f;

                    float norm = static_cast<float>(_strike_peak_delta[i] - 5) / effective_max;
                    norm = daisysp::fclamp(norm, 0.0f, 1.0f);

                    _velocity[i] = daisysp::fclamp(sqrtf(norm), 0.03f, 1.0f);
                    _pressure[i] = target_p;

                    if (_on_touch) _on_touch(i);
                }
            } else {
                _pressure[i] += (target_p - _pressure[i]) * 0.40f;
            }
        } else {
            if (_strike_window[i] > 0) {
                _strike_window[i] = 0;
                if (_strike_peak_delta[i] >= 6 && _release_lockout[i] == 0) {
                    float effective_max = _pad_max_delta[i] - 5.0f;
                    if (effective_max < 30.0f) effective_max = 30.0f;

                    float norm = static_cast<float>(_strike_peak_delta[i] - 5) / effective_max;
                    norm = daisysp::fclamp(norm, 0.0f, 1.0f);

                    _velocity[i] = daisysp::fclamp(sqrtf(norm), 0.03f, 1.0f);
                    _pressure[i] = 0.0f;

                    if (_on_touch) _on_touch(i);
                }
            }

            _pressure[i] += (0.0f - _pressure[i]) * 0.40f;
            if (_pressure[i] < 0.01f) _pressure[i] = 0.0f;

            if (state_changed && !raw_touched) {
                _pressure[i] = 0.0f;
                _release_lockout[i] = 3;
                if (_on_release) _on_release(i);
            }
        }
    }
}
