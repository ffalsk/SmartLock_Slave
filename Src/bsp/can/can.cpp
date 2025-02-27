#include "can.hpp"

namespace bsp {

// 初始化静态成员变量
can_base* can_base::can_instances_[can_base::MAX_CAN_INSTANCES] = {nullptr};
size_t can_base::can_instance_count_                            = 0;

// 自定义中断处理函数
void CAN_Rx_IRQHandler(CAN_HandleTypeDef* hcan, uint32_t fifox) {
    CAN_RxHeaderTypeDef rx_header;
    uint8_t rx_data[8];

    if (HAL_CAN_GetRxMessage(hcan, fifox, &rx_header, rx_data) == HAL_OK) {
        can_base* instance = can_base::get_instance(hcan, rx_header.StdId);
        if (instance) {
            instance->OnRxInterruptCallback(rx_data, rx_header.DLC);
        }
    }
}

} // namespace bsp

extern "C" void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef* hcan) {
    bsp::CAN_Rx_IRQHandler(hcan, CAN_RX_FIFO0);
}

extern "C" void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef* hcan) {
    bsp::CAN_Rx_IRQHandler(hcan, CAN_RX_FIFO1);
}