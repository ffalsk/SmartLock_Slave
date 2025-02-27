#include "gpio.hpp"

namespace bsp
{
    gpio_base *gpio_base::gpio_instances_[MAX_GPIO_INSTANCES] = {nullptr};
    size_t gpio_base::gpio_instance_count_                    = 0;
} // namespace bsp

extern "C" void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    for (size_t i = 0; i < bsp::gpio_base::gpio_instance_count_; ++i) {
        auto instance = bsp::gpio_base::gpio_instances_[i];
        if (instance->GetPin() == GPIO_Pin) {
            instance->OnEXTICallback();
            break;
        }
    }
}