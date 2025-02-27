#include "pwm.hpp"

namespace bsp {

// 静态成员变量初始化
pwm_base* pwm_base::pwm_instances_[pwm_base::MAX_PWM_INSTANCES] = {nullptr};
size_t pwm_base::pwm_instance_count_                            = 0;

} // namespace bsp

extern "C" void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef* htim) {
    using namespace bsp;
    for (size_t i = 0; i < pwm_base::pwm_instance_count_; ++i) {
        auto instance = pwm_base::pwm_instances_[i];
        if (instance->GetHandle() == htim) {
            instance->OnPulseFinishedCallback();
            break;
        }
    }
}