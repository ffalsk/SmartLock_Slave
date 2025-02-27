#pragma once

#include <cstdint>

#include <usart.h>

namespace bsp {

enum class UART_TRANSFER_MODE : uint8_t {
    POLLING = 0,
    IT,
    DMA,
};

// 非模板基类
class uart_base {
public:
    virtual ~uart_base() { unregister_instance(this); }

    virtual void OnRxCpltCallback() = 0;
    [[nodiscard]] UART_HandleTypeDef* GetHandle() const { return uart_handle_; }
    void SetTrueRxSize(uint16_t size) { rx_size_from_register_ = size; }
    [[nodiscard]] uint16_t GetTrueRxSize() const { return rx_size_from_register_; }
    void ReceiveDMAAuto() {
        if (rx_buffer_user != nullptr) {
            HAL_UARTEx_ReceiveToIdle_DMA(uart_handle_, rx_buffer_user, rx_size_);
        } else {
            HAL_UARTEx_ReceiveToIdle_DMA(uart_handle_, rx_buffer_, rx_size_);
        }
        __HAL_DMA_DISABLE_IT(uart_handle_->hdmarx, DMA_IT_HT);
    }
    void Begin() { ReceiveDMAAuto(); }
    uint8_t* GetRxBuffer() { return rx_buffer_; }
    [[nodiscard]] bool IsReady() const { return (uart_handle_->gState != HAL_UART_STATE_BUSY_TX); }

    void Send(uint8_t* send_buf, uint16_t send_size, UART_TRANSFER_MODE mode) {
        switch (mode) {
        case UART_TRANSFER_MODE::POLLING:
            HAL_UART_Transmit(uart_handle_, send_buf, send_size, 100);
            break;
        case UART_TRANSFER_MODE::IT: HAL_UART_Transmit_IT(uart_handle_, send_buf, send_size); break;
        case UART_TRANSFER_MODE::DMA:
            HAL_UART_Transmit_DMA(uart_handle_, send_buf, send_size);
            break;
        default:
            while (true)
                ; // 非法模式，进入死循环
            break;
        }
    }
    void set_dma_rx_buffer(uint8_t* buffer) { rx_buffer_user = buffer; }

protected:
    UART_HandleTypeDef* uart_handle_;
    static constexpr size_t MAX_RX_BUFFER_SIZE = 256;
    uint8_t rx_buffer_[MAX_RX_BUFFER_SIZE];
    uint8_t* rx_buffer_user = nullptr;
    uint16_t rx_size_;
    uint16_t rx_size_from_register_ = 0;

    uart_base(UART_HandleTypeDef* handle, uint16_t rx_size)
        : uart_handle_(handle)
        , rx_size_(rx_size < MAX_RX_BUFFER_SIZE ? rx_size : MAX_RX_BUFFER_SIZE) {
        register_instance(this);
    }

    static void register_instance(uart_base* instance) {
        if (uart_instance_count_ < MAX_UART_INSTANCES) {
            uart_instances_[uart_instance_count_++] = instance;
        }
    }

    static void unregister_instance(uart_base* instance) {
        for (size_t i = 0; i < uart_instance_count_; ++i) {
            if (uart_instances_[i] == instance) {
                for (size_t j = i; j < uart_instance_count_ - 1; ++j) {
                    uart_instances_[j] = uart_instances_[j + 1];
                }
                --uart_instance_count_;
                break;
            }
        }
    }

private:
    static constexpr size_t MAX_UART_INSTANCES = 3;
    static uart_base* uart_instances_[MAX_UART_INSTANCES];
    static size_t uart_instance_count_;

    friend void ::HAL_UARTEx_RxEventCallback(UART_HandleTypeDef* huart, uint16_t Size);
    friend void ::HAL_UART_ErrorCallback(UART_HandleTypeDef* huart);
};

// 模板类
template <typename Derived>
class uart : public uart_base {
public:
    using CallbackFunction = void (Derived::*)();

    struct uart_params {
        UART_HandleTypeDef* uart_handle = nullptr;
        uint16_t recv_buff_size         = MAX_RX_BUFFER_SIZE;
        Derived* callback_instance      = nullptr;
        CallbackFunction callback       = nullptr;
    };

    explicit uart(const uart_params& params)
        : uart_base(params.uart_handle, params.recv_buff_size)
        , callback_instance_(params.callback_instance)
        , callback_function_(params.callback) {}

    void OnRxCpltCallback() override {
        if (callback_instance_ && callback_function_) {
            (callback_instance_->*callback_function_)();
        }
    }

    void SetCallback(Derived* instance, CallbackFunction function) {
        callback_instance_ = instance;
        callback_function_ = function;
    }

private:
    Derived* callback_instance_;
    CallbackFunction callback_function_;
};

} // namespace bsp