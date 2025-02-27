#pragma once
#include <gpio.h>

namespace bsp {

enum class GPIO_EXTI_MODE : uint8_t {
    RISING,
    FALLING,
    RISING_FALLING,
    NONE,
};

// 非模板基类
class gpio_base {
public:
    virtual ~gpio_base() { unregister_instance(this); }
    virtual void OnEXTICallback() = 0;
    [[nodiscard]] uint16_t GetPin() const { return GPIO_Pin_; }

protected:
    GPIO_TypeDef* GPIOx_;
    GPIO_PinState pin_state_;
    GPIO_EXTI_MODE exti_mode_;
    uint16_t GPIO_Pin_;

    gpio_base(
        GPIO_TypeDef* GPIOx, GPIO_PinState pin_state, GPIO_EXTI_MODE exti_mode, uint16_t GPIO_Pin)
        : GPIOx_(GPIOx)
        , pin_state_(pin_state)
        , exti_mode_(exti_mode)
        , GPIO_Pin_(GPIO_Pin) {
        register_instance(this);
    }
    static void unregister_instance(gpio_base* instance) {
        for (size_t i = 0; i < gpio_instance_count_; ++i) {
            if (gpio_instances_[i] == instance) {
                for (size_t j = i; j < gpio_instance_count_ - 1; ++j) {
                    gpio_instances_[j] = gpio_instances_[j + 1];
                }
                --gpio_instance_count_;
                break;
            }
        }
    }
    static void register_instance(gpio_base* instance) {
        if (gpio_instance_count_ < MAX_GPIO_INSTANCES) {
            gpio_instances_[gpio_instance_count_++] = instance;
        }
        // 可以添加错误处理
    }

private:
    // 静态数组保存所有实例
    static constexpr size_t MAX_GPIO_INSTANCES = 16;
    static gpio_base* gpio_instances_[MAX_GPIO_INSTANCES];
    static size_t gpio_instance_count_;

    friend void ::HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin);
};

// 模板类
template <typename Derived>
class gpio : public gpio_base {
public:
    using CallbackFunction = void (Derived::*)();

    struct gpio_params {
        GPIO_TypeDef* GPIOx                = nullptr;
        GPIO_PinState pin_state            = GPIO_PIN_RESET;
        GPIO_EXTI_MODE exti_mode           = GPIO_EXTI_MODE::NONE;
        uint16_t GPIO_Pin                  = 0;
        Derived* callback_instance         = nullptr;
        CallbackFunction callback_function = nullptr;
    };

    explicit gpio(const gpio_params& params)
        : gpio_base(params.GPIOx, params.pin_state, params.exti_mode, params.GPIO_Pin)
        , callback_instance_(params.callback_instance)
        , callback_function_(params.callback_function) {}

    void SetState(GPIO_PinState state) {
        if (pin_state_ != state) {
            pin_state_ = state;
            HAL_GPIO_WritePin(GPIOx_, GPIO_Pin_, pin_state_);
        }
    }
    void Toggle() { HAL_GPIO_TogglePin(GPIOx_, GPIO_Pin_); }

    GPIO_PinState GetState() { return HAL_GPIO_ReadPin(GPIOx_, GPIO_Pin_); }

    void OnEXTICallback() override {
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