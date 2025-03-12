#pragma once

#include <cstdint>

namespace device {
struct __attribute__((packed)) rx_package {
    bool finger_enroll_flag = false;
    bool finger_day_flag    = false;
    bool face_enroll_flag   = false;
    bool power_save_flag    = false;
};
enum class request : uint8_t {
    short_prompt = 0x01,
    long_prompt,
    enroll_prompt,
    wrong_tone,
    success_tone,
    test = 0xFF,
};
} // namespace device