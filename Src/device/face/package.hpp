#pragma once

#include "stm32f1xx_hal.h"
#include "tool/endian_promise.hpp"
#include <cstring>
#include <random>

namespace device {
using namespace tool;
struct __attribute__((packed)) face_package {
    be_uint16_t SOF = 0xEFAA;
    uint8_t MsgID;
    be_uint16_t data_length = 0;
    uint8_t data[128]       = {}; // 末尾加奇偶校验
};
enum class face_result : uint8_t {
    success  = 0,                 // success
    rejected = 1,                 // module rejected this command
    aborted  = 2,                 // algo aborted

    failed_camera         = 4,    // camera open failed
    failed_unknown_reason = 5,    // UNKNOWN_ERROR
    failed_invalid_param  = 6,    // invalid param
    failed_no_memory      = 7,    // no enough memory
    failed_unknown_user   = 8,    // exceed limitation
    failed_max_user       = 9,    // exceed maximum user number
    failed_face_enrolled  = 10,   // this face has been enrolled
    failed_liveness_check = 12,   // liveness check failed
    failed_timeout        = 13,   // exceed the time limit
    failed_authorization  = 14,   // authorization failed
    failed_read_file      = 19,   // read file failed
    failed_write_file     = 20,   // write file failed
    failed_no_encrypt     = 21,   // encrypt must be set
    failed_no_rgbimage    = 23,   // rgb image is not ready
};
struct __attribute__((packed)) face_reply_package {
    be_uint16_t SOF;
    enum class MsgID : uint8_t { reply, note, image } ID;
    be_uint16_t data_length = 0;
    uint8_t mid;
    face_result result;
    uint8_t data[126] = {};       // 末尾加奇偶校验
    void set_zero() { std::memset(this, 0, sizeof(face_reply_package)); }
};

struct __attribute__((packed)) verify_params {
    bool poweroff_after_verify_ = false;
    uint8_t timeout             = 20;
    verify_params& poweroff_after_verify() {
        poweroff_after_verify_ = true;
        return *this;
    }
    verify_params& set_timeout(uint8_t timeout) {
        this->timeout = timeout;
        return *this;
    }
};
struct __attribute__((packed)) enroll_params {
    bool Is_admin = false;
    char name[32];
    enum class face_direction : uint8_t {
        Up        = 0x10,
        Down      = 0x08,
        Left      = 0x04,
        Right     = 0x02,
        Front     = 0x01,
        Undefined = 0x00,
    } direction     = face_direction::Front;
    uint8_t timeout = 20;
    enroll_params() {
        uint32_t seed = HAL_GetTick() ^ *reinterpret_cast<uint32_t*>(UID_BASE);
        std::mt19937 gen(seed);
        std::uniform_int_distribution<> dis(0, 25);
        for (char& i : name) {
            i = dis(gen) + 'a';
        }
    }
    enroll_params& set_direction(face_direction direction) {
        this->direction = direction;
        return *this;
    }
    enroll_params& set_timeout(uint8_t timeout) {
        this->timeout = timeout;
        return *this;
    }
    enroll_params& set_name(const char* name) {
        std::strncpy(this->name, name, sizeof(this->name) - 1);
        return *this;
    }
    enroll_params& admin() {
        Is_admin = true;
        return *this;
    }
};
struct __attribute__((packed)) face_USB_UAC_params {
    uint8_t USB_type     = 0x11;
    bool Is_rotate  : 1  = false;
    bool Is_mirror_ : 1  = false;
    uint8_t unused  : 6  = 0;
    uint8_t jpeg_quality = 99;

    face_USB_UAC_params& USB_20_mode() {
        USB_type = 0x20;
        return *this;
    }
    face_USB_UAC_params& USB_11_mode() {
        USB_type = 0x11;
        return *this;
    }
    face_USB_UAC_params& rotate() {
        Is_rotate = true;
        return *this;
    }
    face_USB_UAC_params& mirror() {
        Is_mirror_ = true;
        return *this;
    }
    face_USB_UAC_params& set_jpeg_quality(uint8_t jpeg_quality) {
        this->jpeg_quality = jpeg_quality;
        return *this;
    }
};
} // namespace device
