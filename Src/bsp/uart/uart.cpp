#include "uart.hpp"

namespace bsp {
// 静态成员变量初始化
uart_base* uart_base::uart_instances_[uart_base::MAX_UART_INSTANCES] = {nullptr};
size_t uart_base::uart_instance_count_                               = 0;
} // namespace bsp

extern "C" void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef* huart, uint16_t Size) {
    using namespace bsp;
    for (size_t i = 0; i < uart_base::uart_instance_count_; ++i) {
        auto instance = uart_base::uart_instances_[i];
        if (instance->GetHandle() == huart) {
            instance->SetTrueRxSize(Size);
            instance->OnRxCpltCallback();
            // instance->ReceiveDMAAuto();
            break;
        }
    }
}

extern "C" void HAL_UART_ErrorCallback(UART_HandleTypeDef* huart) {
    using namespace bsp;
    for (size_t i = 0; i < uart_base::uart_instance_count_; ++i) {
        auto instance = uart_base::uart_instances_[i];
        if (instance->GetHandle() == huart) {
            instance->ReceiveDMAAuto();
            break;
        }
    }
}