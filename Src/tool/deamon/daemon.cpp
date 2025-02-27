#include "daemon.hpp"
#include "bsp/dwt/dwt.h"

namespace tool {
daemon_base* daemon_base::daemon_instances_[daemon_base::MAX_DAEMON_INSTANCES] = {nullptr};
uint8_t daemon_base::daemon_instance_count_                                    = 0;
} // namespace tool

extern "C" void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim) {
    if (htim->Instance == TIM4) {

        for (size_t i = 0; i < tool::daemon_base::daemon_instance_count_; ++i) {
            float current_time = DWT_GetTimeline_s();
            auto instance      = tool::daemon_base::daemon_instances_[i];
            if (instance->state_ == tool::daemon_base::deamon_state::running) {
                if (current_time - instance->last_reload_time_ > instance->dt_) {
                    instance->OnCallback();
                    instance->last_reload_time_ = current_time;
                }
            }
        }
    }
}