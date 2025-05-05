// Copyright (c) 2020 -	Bart Dring
// Use of this source code is governed by a GPLv3 license that can be found in the LICENSE file.

#pragma once

/*
	This is similar to the PWM Spindle except that it enables the
	M4 speed vs. power copensation.
*/

#include "PWMSpindle.h"

#include <cstdint>

namespace Spindles {
    // this is the same as a PWM spindle but the M4 compensation is supported.
    class Laser : public PWM {
    public:
        Laser(const char* name) : PWM(name) {};

        Laser(const Laser&)            = delete;
        Laser(Laser&&)                 = delete;
        Laser& operator=(const Laser&) = delete;
        Laser& operator=(Laser&&)      = delete;

        bool isRateAdjusted() override;
        void config_message() override;
        void init() override;
        void set_direction(bool Clockwise) override {};
        bool use_delay_settings() const override { return false; }

        void group(Configuration::HandlerBase& handler) override {
            // pwm_freq is the only item that the PWM class adds to OnOff
            // We cannot call PWM::group() because that would pick up
            // direction_pin, which we do not want in Laser
            handler.item("pwm_hz", _pwm_freq, 1, 100000);
            handler.item("follow_pin", _follow_pin);
            handler.item("center_pin", _center_pin);
            handler.item("rapup_pin", _rapup_pin);
            handler.item("drill_ok_pin", _drill_ok_pin);
            handler.item("cut_ok_pin", _cut_ok_pin);
            handler.item("back_ok_pin", _back_ok_pin);
            handler.item("alarm_pin", _alarm_pin);
            OnOff::groupCommon(handler);
        }

        Pin _follow_pin;
        Pin _center_pin;
        Pin _rapup_pin;
        Pin _drill_ok_pin;
        Pin _cut_ok_pin;
        Pin _back_ok_pin;
        Pin _alarm_pin;

        ~Laser() {}

        bool laser_start;
        bool follow_hack(uint32_t ms, bool* ret) override;
        bool follow_start() override;
        bool follow_stop() override;

    protected:
        void set_output(uint32_t duty) override;
    };
}
