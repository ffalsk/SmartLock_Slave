#pragma once

#include "app/system_parameters.hpp"
#include "bsp/gpio/gpio.hpp"
#include "bsp/uart/uart.hpp"
#include "device/finger/package.hpp"
#include "tool/deamon/daemon.hpp"
#include "tool/endian_promise.hpp"

#include <cstring>

namespace device {
using namespace tool;
class finger {
public:
    enum class LED_states : uint8_t {
        wrong,
        success,
        waiting,
    };

    struct finger_params {
        bsp::uart<finger>::uart_params uart_params;
        bsp::gpio<finger>::gpio_params INT_params;
        finger_params() {
            uart_params.uart_handle    = &huart1;
            uart_params.recv_buff_size = sizeof(finger_ACK_package);
            INT_params.GPIOx           = GPIOA;
            INT_params.GPIO_Pin        = GPIO_PIN_8;
        }
    };
    explicit finger(const finger_params& params)
        : uart_(params.uart_params)
        , gpio_(params.INT_params)
        , LED_daemon_(1, this, &finger::set_LED_states)
        , EXTI_daemon_(1, this, &finger::allow_verify)
        , enroll_daemon_(20, this, &finger::enroll_fallback) {
        enroll_daemon_.Pause();
        LED_daemon_.Pause();
        uart_.SetCallback(this, &finger::decode_IT_set);
        uart_.set_dma_rx_buffer(reinterpret_cast<uint8_t*>(&rx_package));
        gpio_.SetCallback(this, &finger::identify_IT_set);
    }
    ~finger() = default;
    void Begin() { uart_.Begin(); }

    void auto_enroll(finger_auto_enroll_params data) {
        LED_daemon_.Pause();
        DWT_Delay(2.5);
        app::can_comm_instance->send_request(request::enroll_prompt);
        DWT_Delay(0.5);
        data.ID             = ++user_count_;
        enroll_times_count_ = data.times;
        enroll_success_     = false;
        is_enrolling_       = true;
        app::can_comm_instance->lock_rx_data();
        this->LED_control(finger_led_params().set_mode(LED_modes::AlwaysOff));
        DWT_Delay(0.05);
        enroll_daemon_.Resume();
        this->send_package(0x31, reinterpret_cast<uint8_t*>(&data), sizeof(data));
    }
    void auto_identify(finger_auto_identify_params data) {
        this->send_package(0x32, reinterpret_cast<uint8_t*>(&data), sizeof(data));
    }
    void get_user_count() { this->send_package(0x1D); }

    void set_password(be_uint32_t password) {
        this->send_package(0x12, reinterpret_cast<uint8_t*>(&password), sizeof(password));
    }
    void verify_password() {
        be_uint32_t password = finger_password;
        this->send_package(
            0x13, reinterpret_cast<const uint8_t*>(&password), sizeof(finger_password));
    }
    void set_chip_address(be_uint32_t address) {
        this->send_package(0x15, reinterpret_cast<uint8_t*>(&address), sizeof(address));
    }
    void LED_control(finger_led_params data) {
        this->send_package(0x3C, reinterpret_cast<uint8_t*>(&data), sizeof(data));
    }
    void delete_all() { this->send_package(0x0D); }
    void handshake() { this->send_package(0x35); }
    void sleep() { this->send_package(0x33); }

    void identify() {
        if (!is_enrolling_ && allow_verify_) {
            this->auto_identify(finger_auto_identify_params());
            allow_verify_ = false;
            EXTI_daemon_.Reload();
        }
    }
    void decode() {
        if (size_ < sizeof(finger_ACK_package::header)) {
            reset_state();
            return;
        }

        if (!validate_header(rx_package)) {
            reset_state();
            return;
        }

        if (!validate_checksum(rx_package, size_)) {
            reset_state();
            return;
        }
        process_response(rx_package);

        rx_package.set_zero();
        uart_.ReceiveDMAAuto();
    }

    [[nodiscard]] bool is_received() const { return waiting_ack_cmd_ == 0; }
    [[nodiscard]] uint8_t get_user_count() const { return user_count_; }
    [[nodiscard]] bool is_enroll_success() const { return enroll_success_; }

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

    inline void set_notice(LED_states state) {
        LED_state_ = state;
        LED_daemon_.SetDt(0.2);
        LED_daemon_.Resume();
    }
    void set_LED_to_day() { LED_time_ = LED_time::day; }
    void set_LED_to_night() { LED_time_ = LED_time::night; }
    void set_LED_off() { LED_time_ = LED_time::off; }
    void resume_LED() { LED_daemon_.Resume(); }

private:
    inline void reset_state() { waiting_ack_cmd_ = 0; }
    inline static bool validate_header(const finger_ACK_package& package) {
        return package.header.SOF == 0xEF01 && package.header.address == finger_address;
    }
    inline static bool validate_checksum(const finger_ACK_package& package, uint16_t size) {
        uint16_t checksum_calculated =
            checksum(reinterpret_cast<const uint8_t*>(&package), size - 2);
        uint16_t checksum_received =
            *reinterpret_cast<const be_uint16_t*>(&package.data
                                                       [size - sizeof(finger_ACK_package::header)
                                                        - 2 - sizeof(finger_ACK_package::status)]);
        return checksum_calculated == checksum_received;
    }
    inline void process_response(const finger_ACK_package& package) {
        switch (waiting_ack_cmd_) {
        case 0x32: process_identify_response(package); break;
        case 0x31: process_enroll_response(package); break;
        case 0x1D: process_user_count_response(package); break;
        default: { // 通用处理
            if (package.status != finger_status::OK) {
                // error_handle
            }
            reset_state();
            break;
        }
        }
    }
    inline void process_identify_response(const finger_ACK_package& package) {
        if (package.data[0] == 0x05) {
            if (package.status == finger_status::OK) {
                app::identify_success = true;
                // set_notice(LED_states::success);//不需要，因为在app_main中已经设置了
            } else {
                set_notice(LED_states::wrong);
                app::can_comm_instance->send_request(request::wrong_tone);
            }
            reset_state();
        }
    }
    inline void process_enroll_response(const finger_ACK_package& package) {
        if (package.status != finger_status::OK) {
            is_enrolling_ = false;
            app::can_comm_instance->unlock_rx_data();
            enroll_times_count_ = 0;
            set_notice(LED_states::wrong);
            app::can_comm_instance->send_request(request::wrong_tone);
            enroll_daemon_.Pause();
            reset_state();
            return;
        }

        if (package.data[0] == 0x03) {
            enroll_times_count_--;
            app::can_comm_instance->send_request(request::short_prompt);
        } else if (package.data[0] == 0x02 && enroll_times_count_ == 1) {
            enroll_times_count_--;
            enroll_success_ = true;
            is_enrolling_   = false;
            app::can_comm_instance->unlock_rx_data();
            app::can_comm_instance->send_request(request::long_prompt);
            DWT_Delay(0.5);
            set_notice(LED_states::success);
            enroll_daemon_.Pause();
            reset_state();
        }
    }
    inline void process_user_count_response(const finger_ACK_package& package) {
        user_count_ = package.data[1];
        reset_state();
    }

    void send_package(const uint8_t CMD, const uint8_t* data = nullptr, uint16_t length = 0) {
        // if (waiting_ack_cmd_ != 0)
        //     return;

        waiting_ack_cmd_ = CMD;

        finger_CMD_package package;
        package.header.length = sizeof(package.header.CMD) + length + sizeof(be_uint16_t);
        package.header.CMD    = CMD;

        if (data != nullptr && length > 0)
            std::memcpy(package.data, data, length);

        be_uint16_t checksum_ =
            this->checksum(reinterpret_cast<uint8_t*>(&package), sizeof(package.header) + length);
        std::memcpy(package.data + length, &checksum_, sizeof(checksum_));

        uint8_t watch[128] = {};
        std::memcpy(watch, &package, sizeof(package.header) + length + sizeof(checksum_));

        uart_.Send(
            reinterpret_cast<uint8_t*>(&package),
            sizeof(package.header) + length + sizeof(checksum_), bsp::UART_TRANSFER_MODE::DMA);
    }
    static uint16_t checksum(const uint8_t* data, uint16_t length) {
        constexpr uint8_t checksum_offset =
            sizeof(finger_CMD_package::header.SOF) + sizeof(finger_CMD_package::header.address);
        data += checksum_offset;
        length -= checksum_offset;
        uint64_t sum = 0;
        for (uint16_t i = 0; i < length; ++i) {
            sum += data[i];
        }
        return static_cast<uint16_t>(sum);
    }
    void identify_IT_set() { waiting_identify_ = true; }
    void decode_IT_set() {
        waiting_decode_ = true;
        size_           = uart_.GetTrueRxSize();
        if (is_enrolling_) {
            this->decode();
            waiting_decode_ = false;
        }
    }

    void set_LED_states() {
        static bool waiting_set_to_normal = false;
        switch (LED_state_) {
        case LED_states::wrong: {
            this->LED_control(finger_led_params()
                                  .set_mode(LED_modes::Blink)
                                  .set_loop_times(1)
                                  .set_start_color(LED_colors::Red));
            waiting_set_to_normal = true;
            break;
        }
        case LED_states::success: {
            this->LED_control(finger_led_params()
                                  .set_mode(LED_modes::Blink)
                                  .set_loop_times(1)
                                  .set_start_color(LED_colors::Green));
            waiting_set_to_normal = true;
            break;
        }
        case LED_states::waiting: {
            if (LED_time_ == LED_time::day)
                this->LED_control(finger_led_params()
                                      .set_mode(LED_modes::Breath)
                                      .set_start_color(LED_colors::GreenBlue)
                                      .set_end_color(LED_colors::GreenBlue));
            else if (LED_time_ == LED_time::night)
                this->LED_control(finger_led_params()
                                      .set_mode(LED_modes::Breath)
                                      .set_start_color(LED_colors::RedBlue)
                                      .set_end_color(LED_colors::RedBlue));
            else
                this->LED_control(finger_led_params().set_mode(LED_modes::AlwaysOff));

            break;
        }
        };
        LED_state_ = LED_states::waiting;
        LED_daemon_.SetDt(2);
        if (waiting_set_to_normal) {
            LED_daemon_.SetDt(0.8);
            waiting_set_to_normal = false;
        }
    }
    void enroll_fallback() {
        is_enrolling_ = false;
        app::can_comm_instance->unlock_rx_data();
        enroll_times_count_ = 0;
        set_notice(LED_states::wrong);
        app::can_comm_instance->send_request(request::wrong_tone);
        enroll_daemon_.Pause();
        reset_state();
    }
    void allow_verify() { allow_verify_ = true; }
    bsp::uart<finger> uart_;
    bsp::gpio<finger> gpio_;
    tool::daemon<finger> LED_daemon_;
    tool::daemon<finger> EXTI_daemon_;
    tool::daemon<finger> enroll_daemon_;
    // tool::daemon<finger> cmd_waiting_daemon_;

    uint8_t waiting_ack_cmd_ = 0;
    uint8_t user_count_      = 0;

    uint8_t enroll_times_count_ = 0;
    bool is_enrolling_          = false;
    bool enroll_success_        = false;

    bool allow_verify_ = true;

    bool waiting_identify_ = false;
    bool waiting_decode_   = false;

    uint16_t size_                = 0;
    finger_ACK_package rx_package = {};

    LED_states LED_state_ = LED_states::waiting;
    enum class LED_time : uint8_t {
        day,
        night,
        off,
    } LED_time_ = LED_time::night;
};
} // namespace device