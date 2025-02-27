#pragma once

#include <can.h>

#include <cstdint>

namespace bsp {

// 非模板基类
class can_base {
public:
    virtual ~can_base() { unregister_instance(this); }

    // 纯虚函数，派生类需要实现
    virtual void OnRxInterruptCallback(uint8_t* data, uint8_t length) = 0;

    void Begin() {
        AddFilters();
        if (can_instance_count_ == 1) {
            ServiceInit();
        }
    }
    // 发送 CAN 消息
    void Transmit(uint8_t* tx_data, uint8_t length, uint32_t id = 0) {
        CAN_TxHeaderTypeDef tx_header;
        uint32_t tx_mailbox;

        tx_header.StdId              = id != 0 ? id : tx_id_; // 发送ID
        tx_header.RTR                = CAN_RTR_DATA;          // 数据帧
        tx_header.IDE                = CAN_ID_STD;            // 标准帧
        tx_header.DLC                = length;                // 数据长度
        tx_header.TransmitGlobalTime = DISABLE;

        if (HAL_CAN_AddTxMessage(can_handle_, &tx_header, tx_data, &tx_mailbox) != HAL_OK) {
            // 发送错误处理
        }
    }

    // 查找实例
    static can_base* get_instance(CAN_HandleTypeDef* hcan, uint32_t rx_id) {
        for (size_t i = 0; i < can_instance_count_; ++i) {
            if (can_instances_[i]->can_handle_ == hcan && can_instances_[i]->rx_id_ == rx_id) {
                return can_instances_[i];
            }
        }
        return nullptr;
    }

protected:
    CAN_HandleTypeDef* can_handle_;
    uint32_t tx_id_;
    uint32_t rx_id_;

    can_base(CAN_HandleTypeDef* _hcan, uint32_t _tx_id, uint32_t _rx_id)
        : can_handle_(_hcan)
        , tx_id_(_tx_id)
        , rx_id_(_rx_id) {
        register_instance(this);
    }

    void AddFilters() {
        CAN_FilterTypeDef filter;
        filter.FilterIdHigh     = (rx_id_ << 5) & 0xFFFF; // 标准ID位于位15:5
        filter.FilterIdLow      = 0x0000;
        filter.FilterMaskIdHigh = 0xFFFF;                 // 精确匹配ID
        filter.FilterMaskIdLow  = 0x0000;
        filter.FilterFIFOAssignment =
            (rx_id_ & 1) ? CAN_RX_FIFO0 : CAN_RX_FIFO1;   // 奇数ID -> FIFO0, 偶数ID -> FIFO1
        filter.FilterBank =
            static_cast<uint32_t>(can_instance_count_ - 1); // 每个实例使用不同的过滤器bank
        filter.FilterMode       = CAN_FILTERMODE_IDMASK;
        filter.FilterScale      = CAN_FILTERSCALE_32BIT;
        filter.FilterActivation = ENABLE;

        if (HAL_CAN_ConfigFilter(can_handle_, &filter) != HAL_OK) {
            // 过滤器配置错误处理
        }
    }
    static void ServiceInit() {
        if (HAL_CAN_Start(&hcan) != HAL_OK) {
            // 启动错误处理
        }
        if (HAL_CAN_ActivateNotification(
                &hcan, CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_RX_FIFO1_MSG_PENDING)
            != HAL_OK) {
            // 中断激活错误处理
        }
    }

    // 注册和注销实例
    static void register_instance(can_base* instance) {
        if (can_instance_count_ < MAX_CAN_INSTANCES) {
            can_instances_[can_instance_count_++] = instance;
        }
    }

    static void unregister_instance(can_base* instance) {
        for (size_t i = 0; i < can_instance_count_; ++i) {
            if (can_instances_[i] == instance) {
                for (size_t j = i; j < can_instance_count_ - 1; ++j) {
                    can_instances_[j] = can_instances_[j + 1];
                }
                can_instances_[can_instance_count_ - 1] = nullptr;
                --can_instance_count_;
                break;
            }
        }
    }

private:
    static constexpr size_t MAX_CAN_INSTANCES = 14; // STM32F103 有 14 个过滤器
    static can_base* can_instances_[MAX_CAN_INSTANCES];
    static size_t can_instance_count_;

    // 友元函数，用于中断处理
    friend void CAN_Rx_IRQHandler(CAN_HandleTypeDef* hcan);
};

// 模板类，使用 CRTP 进行回调绑定
template <typename Derived>
class can : public can_base {
public:
    using CallbackFunction = void (Derived::*)(uint8_t* data, uint8_t length);

    struct can_params {
        CAN_HandleTypeDef* can_handle = nullptr; // CAN句柄
        uint32_t tx_id                = 0;       // 发送ID
        uint32_t rx_id                = 0;       // 接收ID
        Derived* callback_instance    = nullptr; // 回调实例
        CallbackFunction callback     = nullptr; // 回调函数
    };

    explicit can(const can_params& params)
        : can_base(params.can_handle, params.tx_id, params.rx_id)
        , callback_instance_(params.callback_instance)
        , callback_function_(params.callback) {}

    // 实现基类的纯虚函数，调用上层类的回调
    void OnRxInterruptCallback(uint8_t* data, uint8_t length) override {
        if (callback_instance_ && callback_function_) {
            (callback_instance_->*callback_function_)(data, length);
        }
    }

    // 设置回调
    void SetCallback(Derived* instance, CallbackFunction function) {
        callback_instance_ = instance;
        callback_function_ = function;
    }

private:
    Derived* callback_instance_;
    CallbackFunction callback_function_;
};

} // namespace bsp