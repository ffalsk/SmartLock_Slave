#pragma once
#include <cstdint>

#include <tim.h>


namespace bsp {

// 非模板基类
class pwm_base {
public:
    virtual ~pwm_base() {
        unregister_instance(this);
        Stop();
    }
    void Begin() {
        tclk_ = SelectTclk(htim_);
        Start();
        SetPeriod(period_);
        SetDutyCycle(duty_cycle_);
    }

    virtual void OnPulseFinishedCallback() = 0;

    [[nodiscard]] TIM_HandleTypeDef* GetHandle() const { return htim_; }
    [[nodiscard]] uint32_t GetChannel() const { return channel_; }

protected:
    TIM_HandleTypeDef* htim_;
    uint32_t channel_;
    float period_;
    float duty_cycle_;
    uint32_t tclk_;                      // 时钟频率

    pwm_base(TIM_HandleTypeDef* htim, uint32_t channel, float period, float duty_cycle)
        : htim_(htim)
        , channel_(channel)
        , period_(period)
        , duty_cycle_(duty_cycle) {
        register_instance(this);
    }

    void Start() { HAL_TIM_PWM_Start(htim_, channel_); }

    void Stop() { HAL_TIM_PWM_Stop(htim_, channel_); }

    void SetPeriod(float period) {
        period_  = period;
        auto arr = static_cast<uint16_t>(
            period_ * static_cast<float>(tclk_) / (htim_->Init.Prescaler + 1.0));
        __HAL_TIM_SET_AUTORELOAD(htim_, arr);
        __HAL_TIM_SET_COUNTER(htim_, 0); // 改变周期后重置计数器
    }

    void SetDutyCycle(float duty_cycle) {
        duty_cycle_  = duty_cycle;
        auto compare = static_cast<uint16_t>(duty_cycle_ * (__HAL_TIM_GET_AUTORELOAD(htim_) + 1.0));
        __HAL_TIM_SET_COMPARE(htim_, channel_, compare);
    }

    static uint32_t SelectTclk(TIM_HandleTypeDef* htim) {
        uint32_t pclk;
        uint32_t tim_clk;
        if (htim->Instance == TIM1) {
            // TIM1 位于 APB2
            pclk    = HAL_RCC_GetPCLK2Freq();
            tim_clk = pclk;

        } else {
            // 其他定时器位于 APB1
            pclk    = HAL_RCC_GetPCLK1Freq();
            tim_clk = pclk * 2;
        }
        return tim_clk;
    }

    static void register_instance(pwm_base* instance) {
        if (pwm_instance_count_ < MAX_PWM_INSTANCES) {
            pwm_instances_[pwm_instance_count_++] = instance;
        }
    }

    static void unregister_instance(pwm_base* instance) {
        for (size_t i = 0; i < pwm_instance_count_; ++i) {
            if (pwm_instances_[i] == instance) {
                for (size_t j = i; j < pwm_instance_count_ - 1; ++j) {
                    pwm_instances_[j] = pwm_instances_[j + 1];
                }
                --pwm_instance_count_;
                break;
            }
        }
    }

private:
    static constexpr size_t MAX_PWM_INSTANCES = 16;
    static pwm_base* pwm_instances_[MAX_PWM_INSTANCES];
    static size_t pwm_instance_count_;

    friend void ::HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef* htim);
};

// 模板类
template <typename Derived>
class pwm : public pwm_base {
public:
    using CallbackFunction = void (Derived::*)();

    struct pwm_params {
        TIM_HandleTypeDef* htim    = nullptr;       // TIM句柄
        uint32_t channel           = TIM_CHANNEL_1; // 通道
        float period               = 0.0f;          // 周期（秒）
        float duty_cycle           = 0.0f;          // 占空比（0.0 - 1.0）
        Derived* callback_instance = nullptr;       // 回调实例
        CallbackFunction callback  = nullptr;       // 回调函数
    };

    explicit pwm(const pwm_params& params)
        : pwm_base(params.htim, params.channel, params.period, params.duty_cycle)
        , callback_instance_(params.callback_instance)
        , callback_function_(params.callback) {}

    void OnPulseFinishedCallback() override {
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