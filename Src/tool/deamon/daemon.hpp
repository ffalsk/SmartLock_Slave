#pragma once
#include "bsp/dwt/dwt.h"

#include <tim.h>

#include <cstddef>
#include <cstdint>

namespace tool {

class daemon_base {
    enum class deamon_state : uint8_t {
        running,
        paused,
    };

public:
    virtual ~daemon_base() { unregister_instance(this); }
    virtual void OnCallback() = 0;

    void SetDt(float dt) { dt_ = dt; }
    void Reload() { last_reload_time_ = DWT_GetTimeline_s(); }
    void Pause() { state_ = deamon_state::paused; }
    void Resume() {
        state_ = deamon_state::running;
        Reload();
    }
    [[nodiscard]] bool IsPaused() const { return state_ == deamon_state::paused; }

protected:
    float dt_;
    float last_reload_time_ = 0;
    deamon_state state_     = deamon_state::running;

    explicit daemon_base(float dt)
        : dt_(dt) {
        register_instance(this);
    }

    static void unregister_instance(daemon_base* instance) {
        for (uint8_t i = 0; i < daemon_instance_count_; ++i) {
            if (daemon_instances_[i] == instance) {
                for (uint8_t j = i; j < daemon_instance_count_ - 1; ++j) {
                    daemon_instances_[j] = daemon_instances_[j + 1];
                }
                --daemon_instance_count_;
                break;
            }
        }
    }

    static void register_instance(daemon_base* instance) {
        if (daemon_instance_count_ < MAX_DAEMON_INSTANCES) {
            daemon_instances_[daemon_instance_count_++] = instance;
        }
        // 可以添加错误处理
    }

private:
    static constexpr size_t MAX_DAEMON_INSTANCES = 25;
    static daemon_base* daemon_instances_[MAX_DAEMON_INSTANCES];
    static uint8_t daemon_instance_count_;

    friend void ::HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim);
};

template <typename Derived>
class daemon : public daemon_base {
public:
    using CallbackFunction = void (Derived::*)();

    explicit daemon(const float dt, Derived* instance, CallbackFunction function)
        : daemon_base(dt)
        , callback_instance_(instance)
        , callback_function_(function) {}

    void OnCallback() override {
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
} // namespace tool