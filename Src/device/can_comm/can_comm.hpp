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
        can_.Transmit(&tx_data, sizeof(tx_data));
    }

    [[nodiscard]] bool get_finger_enroll_flag() {
        if (rx_package_.finger_enroll_flag) {
            rx_package_.finger_enroll_flag = false;
            return true;
        }
        return false;
    }
    [[nodiscard]] bool get_finger_day_flag() const { return rx_package_.finger_day_flag; }
    [[nodiscard]] bool get_face_enroll_flag() {
        if (rx_package_.face_enroll_flag) {
            rx_package_.face_enroll_flag = false;
            return true;
        }
        return false;
    }
    void lock_rx_data() { data_locked_ = true; }
    void unlock_rx_data() { data_locked_ = false; }

private:
    void decode(uint8_t* rx_data, uint8_t length) {
        if (length != sizeof(rx_package)) {
            return;
        }
        std::memcpy(&rx_package_, rx_data, length);
    }

    bool data_locked_      = false;
    rx_package rx_package_ = {};
    bsp::can<can_comm> can_;
};
} // namespace device