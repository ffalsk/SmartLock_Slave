#pragma once

#include "bsp/can/can.hpp"
#include "package.hpp"

#include <cstring>

namespace device {
class can_comm {
public:
    struct can_comm_params {
        bsp::can<can_comm>::can_params can_params;
        can_comm_params() {
            can_params.can_handle = &hcan;
            can_params.tx_id      = 0x102;
            can_params.rx_id      = 0x101;
        }
    };
    explicit can_comm(const can_comm_params& params)
        : can_(params.can_params) {
        can_.SetCallback(this, &can_comm::decode);
    }
    ~can_comm() = default;
    void Begin() { can_.Begin(); }
    void send_identify_success() {
        uint8_t tx_data = 0x01;
        can_.Transmit(&tx_data, sizeof(tx_data), 0x100); // 最高优先级
    }
    void send_request(request req) {
        auto tx_data = static_cast<uint8_t>(req);
        can_.Transmit(&tx_data, sizeof(tx_data), 0x102);
    }
    void send_status(uint8_t seq, uint8_t status) {
        uint8_t tx_data[2] = {seq, status};
        can_.Transmit(tx_data, sizeof(tx_data), 0x104);
    }

    [[nodiscard]] bool get_finger_enroll_flag() {
        if (rx_status_.finger_enroll_flag) {
            rx_status_.finger_enroll_flag = false;
            return true;
        }
        return false;
    }
    [[nodiscard]] bool get_finger_day_flag() const { return rx_status_.finger_day_flag; }
    [[nodiscard]] bool get_power_save_flag() const { return rx_status_.power_save_flag; }
    [[nodiscard]] bool get_door_open_flag() const { return rx_status_.door_open_flag; }
    [[nodiscard]] bool get_face_enroll_flag() {
        if (rx_status_.face_enroll_flag) {
            rx_status_.face_enroll_flag = false;
            return true;
        }
        return false;
    }
    void lock_rx_data() { data_locked_ = true; }
    void unlock_rx_data() { data_locked_ = false; }

private:
    void decode(uint8_t* rx_data, uint8_t length) {
        if (length % 2 != 0) {
            // 数据长度必须是偶数
            return;
        }
        if (data_locked_) {
            // 如果数据被锁定，则不进行解码
            return;
        }
        auto len      = length;
        auto data_ptr = rx_data;
        while (len > 0) {
            if (data_ptr[0] <= sizeof(rx_status_)) {
                reinterpret_cast<uint8_t*>(&rx_status_)[data_ptr[0] - 1] = data_ptr[1];
            }
            len -= 2;
            data_ptr += 2;
        }
    }

    bool data_locked_    = false;
    rx_status rx_status_ = {};
    bsp::can<can_comm> can_;
};
} // namespace device