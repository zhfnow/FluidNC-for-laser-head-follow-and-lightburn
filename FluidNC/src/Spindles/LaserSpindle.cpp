// Copyright (c) 2020 -	Bart Dring
// Use of this source code is governed by a GPLv3 license that can be found in the LICENSE file.

/*
    This is similar to the PWM Spindle except that it enables the
    M4 speed vs. power compensation.
*/

#include "LaserSpindle.h"
#include "esp_timer.h"    // Use ESP-IDF timer for timeouts
#include "../Protocol.h"  // Include for send_alarm and ExecAlarm

#include "../Machine/MachineConfig.h"

// ===================================== Laser ==============================================

namespace Spindles {
    bool Laser::isRateAdjusted() {
        return true;  // can use M4 (CCW) laser mode.
    }

    void Laser::config_message() {
        log_info(name() << " Ena:" << _enable_pin.name() << " Out:" << _output_pin.name() << " Freq:" << _pwm_freq
                        << "Hz Period:" << _output_pin.maxDuty() << atc_info()  //
                        << " follow_pin" << _follow_pin.name()                  //
                        << " center_pin" << _center_pin.name()                  //
                        << " rapup_pin" << _rapup_pin.name()                    //
                        << " drill_ok_pin" << _drill_ok_pin.name()              //
                        << " cut_ok_pin" << _cut_ok_pin.name()                  //
                        << " back_ok_pin" << _back_ok_pin.name()                //
                        << " alarm_pin" << _alarm_pin.name());
    }

    void Laser::init() {
        if (_speeds.size() == 0) {
            // The default speed map for a Laser is linear from 0=0% to 255=100%
            linearSpeeds(255, 100.0f);
        }
        // A speed map is now present and PWM::init() will not set its own default

        laser_start = false;

        _follow_pin.setAttr(Pin::Attr::Output);
        _center_pin.setAttr(Pin::Attr::Output);
        _rapup_pin.setAttr(Pin::Attr::Output);
        _drill_ok_pin.setAttr(Pin::Attr::Input);
        _cut_ok_pin.setAttr(Pin::Attr::Input);
        _back_ok_pin.setAttr(Pin::Attr::Input);
        _alarm_pin.setAttr(Pin::Attr::Input);

        _follow_pin.write(false);
        _center_pin.write(false);
        _rapup_pin.write(false);

        PWM::init();

        // Turn off is_reversable regardless of what PWM::init() thinks.
        // Laser mode uses M4 for speed-dependent power instead of CCW rotation.
        is_reversable = false;
    }

    void IRAM_ATTR Laser::set_output(uint32_t duty) {
        // to prevent excessive calls to pwmSetDuty, make sure duty has changed
        if (duty == _current_pwm_duty) {
            return;
        }

        _current_pwm_duty = duty;

        if (laser_start || duty == 0) {
            _output_pin.setDuty(duty);
        }

        log_info(name() << " set out:" << duty);
    }

    bool Laser::follow_start() {
        // 启动
        _follow_pin.write(true);
        // 等待到达穿孔位
        log_info(name() << " wait drill ...... ");
        uint64_t drill_start_time_us = esp_timer_get_time();
        while (!_drill_ok_pin.read()) {
            if ((esp_timer_get_time() - drill_start_time_us) > 1000000) {  // 1000ms = 1,000,000 us
                log_error(name() << " wait drill timeout!");
                send_alarm(ExecAlarm::SpindleControl);  // Set system alarm state
                _follow_pin.write(false);               // Ensure follow pin is turned off on timeout
                return false;
            }
            delay_ms(1);  // Yield/delay slightly
        }
        log_info(name() << " wait drill ok");

        // 开激光
        laser_start = true;
        _output_pin.setDuty(_current_pwm_duty);
        log_info(name() << " laser start out:" << _current_pwm_duty);

        // 等待到达切割位
        log_info(name() << " wait cut ...... ");
        uint64_t cut_start_time_us = esp_timer_get_time();
        while (!_cut_ok_pin.read()) {
            if ((esp_timer_get_time() - cut_start_time_us) > 1000000) {  // 1000ms = 1,000,000 us
                log_error(name() << " wait cut timeout!");
                send_alarm(ExecAlarm::SpindleControl);  // Set system alarm state
                // 停止激光并尝试回中
                laser_start = false;
                _output_pin.setDuty(0);  // Turn off laser immediately
                _follow_pin.write(false);
                return false;
            }
            delay_ms(1);  // Yield/delay slightly
        }
        log_info(name() << " wait cut ok");
        return true;
    }

    bool Laser::follow_stop() {
        // 停止激光
        laser_start = false;
        _output_pin.setDuty(0);  // Turn off laser immediately

        // 通知回中
        _follow_pin.write(false);
        // 等待回中
        log_info(name() << " wait back ...... ");

        // back ok没有反馈，只能直接延迟
        delay_ms(200);

        // while (!_back_ok_pin.read()) {
        //     delay_msec(1, DwellMode::Dwell);
        // }
        log_info(name() << " wait back ok");
        return true;
    }

    bool Laser::follow_hack(uint32_t ms, bool* ret) {
        if (ms == 333) {
            *ret = follow_start();
            return true;
        } else if (ms == 444) {
            *ret = follow_stop();
            return true;
        } else {
            return false;
        }
    }

    // Configuration registration
    namespace {
        SpindleFactory::InstanceBuilder<Laser> registration("Laser");
    }
}
