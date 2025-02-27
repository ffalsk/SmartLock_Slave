#pragma once

#include "app/system_parameters.hpp"
#include "bsp/dwt/dwt.h"
#include "bsp/gpio/gpio.hpp"
#include "bsp/uart/uart.hpp"
#include "device/face/package.hpp"
#include "device/finger/finger.hpp"

#include <array>
#include <cstring>

namespace device {
class face {
public:
    enum class face_states : uint8_t { idle, busy, error, invalid };
    struct face_params {
        bsp::uart<face>::uart_params uart_params;
        bsp::gpio<face>::gpio_params INT_params;
        face_params() {
            uart_params.uart_handle    = &huart2;
            uart_params.recv_buff_size = sizeof(face_reply_package);
            INT_params.GPIOx           = GPIOB;
            INT_params.GPIO_Pin        = GPIO_PIN_14;
        }
    };
    explicit face(const face_params& params)
        : uart_(params.uart_params)
        , gpio_(params.INT_params)
        , identify_daemon_(21, this, &face::identify) {
        uart_.SetCallback(this, &face::decode_IT_set);
        uart_.set_dma_rx_buffer(reinterpret_cast<uint8_t*>(&rx_package));
        gpio_.SetCallback(this, &face::human_detect_IT_set);
        identify_daemon_.Pause();
    }
    ~face() = default;
    void Begin() { uart_.Begin(); }

    void enroll_interactive() {
        // clang-format off
        constexpr std::array<enroll_params::face_direction, 5> directions = {
            enroll_params::face_direction::Front,
            enroll_params::face_direction::Up,
            enroll_params::face_direction::Down,
            enroll_params::face_direction::Left,
            enroll_params::face_direction::Right
        };
        // clang-format on
        enroll_params params = {};
        // this->reset();
        DWT_Delay(2.5);
        app::can_comm_instance->send_request(request::enroll_prompt);
        DWT_Delay(0.5);
        for (const auto& dir : directions) {
            enroll_unexpected_exit_ = false;
            enroll_single_success_  = false;
            params.set_direction(dir);
            this->enroll(params);
            is_enrolling_ = true;
            app::can_comm_instance->lock_rx_data();
            auto now                = DWT_GetTimeline_s();
            constexpr float timeout = 20.0f;
            while (!enroll_single_success_) {
                if (enroll_unexpected_exit_) {
                    if (finger_) {
                        finger_->set_notice(finger::LED_states::wrong);
                        app::can_comm_instance->send_request(request::wrong_tone);
                    }
                    is_enrolling_ = false;
                    app::can_comm_instance->unlock_rx_data();
                    return;
                }
                if (DWT_GetTimeline_s() - now > timeout) {
                    this->reset();
                    if (finger_) {
                        finger_->set_notice(finger::LED_states::wrong);
                        app::can_comm_instance->send_request(request::wrong_tone);
                    }
                    is_enrolling_ = false;
                    app::can_comm_instance->unlock_rx_data();
                    return;
                }
            }
            if (finger_) {
                app::can_comm_instance->send_request(request::short_prompt);
                finger_->set_notice(finger::LED_states::success);
            }
            DWT_Delay(2);
        }
        app::can_comm_instance->send_request(request::long_prompt);
        is_enrolling_ = false;
        app::can_comm_instance->unlock_rx_data();
    }

    void enroll(enroll_params data) {
        this->send_package(0x13, reinterpret_cast<uint8_t*>(&data), sizeof(data));
    }
    void verify(verify_params data) {
        this->send_package(0x12, reinterpret_cast<uint8_t*>(&data), sizeof(data));
    }

    void reset() { this->send_package(0x10); }
    void get_status() { this->send_package(0x11); }
    void delete_all() { this->send_package(0x21); }
    void set_USB_UVC_parameters(face_USB_UAC_params data) {
        this->send_package(0xB1, reinterpret_cast<uint8_t*>(&data), sizeof(data));
    }

    void bind_finger(device::finger* finger) { finger_ = finger; }

    void identify() {
        if (app::human_detected) {
            identify_daemon_.Resume();
        } else {
            identify_daemon_.Pause();
        }
        if (is_enrolling_ == false) {
            this->verify(verify_params());
        }
    }
    void decode() {
        if (size < sizeof(face_package::SOF) + sizeof(face_package::MsgID)
                       + sizeof(face_package::data_length) + sizeof(uint8_t)) {
            return;
        }

        if (!validate_header(rx_package)) {
            return;
        }

        if (!validate_parity_check(rx_package, size)) {
            return;
        }
        process_response(rx_package);

        rx_package.set_zero();
        uart_.ReceiveDMAAuto();
    }

    [[nodiscard]] bool is_init_finished() const { return Init_finished_; }
    [[nodiscard]] face_states get_face_state() const { return face_state_; }
    [[nodiscard]] bool is_waiting_identify() {
        if (waiting_identify_) {
            waiting_identify_ = false;
            return true;
        }
        return false;
    }
    [[nodiscard]] bool is_waiting_decode() {
        if (waiting_decode_) {
            waiting_decode_ = false;
            return true;
        }
        return false;
    }

private:
    inline static bool validate_header(const face_reply_package& package) {
        return package.SOF == 0xEFAA;
    }
    inline static bool validate_parity_check(const face_reply_package& package, uint16_t size) {
        uint8_t parity_check_ = parity_check(reinterpret_cast<const uint8_t*>(&package), size - 1);
        uint8_t offset        = package.data_length;
        offset -= sizeof(face_reply_package::result) + sizeof(face_reply_package::mid);
        return parity_check_ == package.data[offset];       // attention big endian bug
    }
    inline void process_response(const face_reply_package& package) {
        if (package.ID == face_reply_package::MsgID::image) // 暂不处理图像信息
            return;
        else if (package.ID == face_reply_package::MsgID::note) {
            // 仅处理ready消息
            if (package.mid == 0) {
                Init_finished_ = true;
            }
        } else if (package.ID == face_reply_package::MsgID::reply) {
            switch (package.mid) {
            case 0x11: process_get_status_response(package); break;
            case 0x12: process_verify_response(package); break;
            case 0x13: process_enroll_response(package); break;
            default: break;
            }
        }
    }
    inline void process_get_status_response(const face_reply_package& package) {
        face_state_ = static_cast<face_states>(package.data[0]);
    }
    static inline void process_verify_response(const face_reply_package& package) {
        if (package.result == face_result::success) {
            app::identify_success = true;
        }
    }
    inline void process_enroll_response(const face_reply_package& package) {
        if (package.result == face_result::success) {
            enroll_single_success_ = true;
        } else {
            enroll_unexpected_exit_ = true;
            this->reset();
        }
    }
    void send_package(const uint8_t MsgID, const uint8_t* data = nullptr, uint16_t length = 0) {
        face_package package;
        package.data_length = length;
        package.MsgID       = MsgID;

        if (data != nullptr && length > 0)
            std::memcpy(package.data, data, length);

        constexpr uint8_t header_length = sizeof(face_package::SOF) + sizeof(face_package::MsgID)
                                        + sizeof(face_package::data_length);
        uint8_t parity_check_ =
            this->parity_check(reinterpret_cast<uint8_t*>(&package), header_length + length);
        package.data[length] = parity_check_;
        uart_.Send(
            reinterpret_cast<uint8_t*>(&package), header_length + length + sizeof(parity_check_),
            bsp::UART_TRANSFER_MODE::DMA);
    }
    static uint8_t parity_check(const uint8_t* data, uint16_t length) {
        constexpr uint8_t parity_check_offset = sizeof(face_package::SOF);
        data += parity_check_offset;
        length -= parity_check_offset;
        uint8_t parity = 0;
        for (uint16_t i = 0; i < length; i++) {
            parity ^= data[i];
        }
        return parity;
    }
    void human_detect_IT_set() {
        app::human_detected = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_14);
        if (app::human_detected) {
            waiting_identify_ = true;
        }
    }
    void decode_IT_set() {
        waiting_decode_ = true;
        size            = uart_.GetTrueRxSize();
        if (is_enrolling_) {
            this->decode();
            waiting_decode_ = false;
        }
    }
    bsp::uart<face> uart_;
    bsp::gpio<face> gpio_;
    device::finger* finger_ = nullptr;
    tool::daemon<face> identify_daemon_;

    face_states face_state_ = face_states::idle;
    bool Init_finished_     = false;

    bool waiting_identify_ = false;
    bool waiting_decode_   = false;

    uint16_t size                 = 0;
    face_reply_package rx_package = {};

    bool enroll_unexpected_exit_ = false;
    bool enroll_single_success_  = false;
    bool is_enrolling_           = false;
};
} // namespace device
