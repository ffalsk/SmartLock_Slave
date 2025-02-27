#pragma once

#include "tool/endian_promise.hpp"

#include <cstdint>
#include <cstring>

namespace device {
using namespace tool;
constexpr uint32_t finger_address  = 0xFFFFFFFF;
constexpr uint32_t finger_password = 0x78641644;
struct __attribute__((packed)) finger_CMD_package {
    struct __attribute__((packed)) {
        be_uint16_t SOF     = 0xEF01;
        be_uint32_t address = finger_address;
        uint8_t ID          = 0x01; // CMD
        be_uint16_t length  = 0;
        uint8_t CMD         = 0x00;
    } header;
    uint8_t data[128] = {};         // 末尾加上校验和
};

enum class finger_status : uint8_t {
    OK               = 0x00,        // 00H：表示指令执行完毕或 OK
    PacketError      = 0x01,        // 01H：表示数据包接收错误
    NoFinger         = 0x02,        // 02H：表示传感器上没有手指
    ImageCaptureFail = 0x03,        // 03H：表示录入指纹图像失败
    ImageTooDry      = 0x04,        // 04H：表示指纹图像太干、太淡而生不成特征
    ImageTooWet      = 0x05,        // 05H：表示指纹图像太湿、太糊而生不成特征
    ImageTooMessy    = 0x06,        // 06H：表示指纹图像太乱而生不成特征
    FewFeaturePoints = 0x07, // 07H：表示指纹图像正常，但特征点太少（或面积太小）而生不成特征
    FingerNotMatch     = 0x08, // 08H：表示指纹不匹配
    NoFingerFound      = 0x09, // 09H：表示没搜索到指纹
    MergeFail          = 0x0A, // 0AH：表示特征合并失败
    AddressOutOfRange  = 0x0B, // 0BH：表示访问指纹库时地址序号超出指纹库范围
    ReadTemplateFail   = 0x0C, // 0CH：表示从指纹库读模板出错或无效
    UploadFeatureFail  = 0x0D, // 0DH：表示上传特征失败
    CannotReceiveData  = 0x0E, // 0EH：表示模组不能接收后续数据包
    UploadImageFail    = 0x0F, // 0FH：表示上传图像失败
    DeleteTemplateFail = 0x10, // 10H：表示删除模板失败
    ClearLibraryFail   = 0x11, // 11H：表示清空指纹库失败
    EnterLowPowerFail  = 0x12, // 12H：表示不能进入低功耗状态
    WrongPassword      = 0x13, // 13H：表示口令不正确
    NoValidImage       = 0x15, // 15H：表示缓冲区内没有有效原始图而生不成图像
    UpgradeFail        = 0x16, // 16H：表示在线升级失败
    FingerNotMoved     = 0x17, // 17H：表示残留指纹或两次釆集之间手指没有移动过
    FlashError         = 0x18, // 18H：表示读写 FLASH 出错
    RandomNumberFail   = 0x19, // 19H：随机数生成失败
    InvalidRegister    = 0x1A, // 1AH：无效寄存器号
    RegisterSettingError = 0x1B, // 1BH：寄存器设定内容错误号
    NotebookPageError    = 0x1C, // 1CH：记事本页码指定错误
    PortOperationFail    = 0x1D, // 1DH：端口操作失败
    EnrollFail           = 0x1E, // 1EH：自动注册（enroll）失败
    LibraryFull          = 0x1F, // 1FH：指纹库满
    WrongDeviceAddress   = 0x20, // 20H：设备地址错误
    PasswordError        = 0x21, // 21H：密码有误
    TemplateNotEmpty     = 0x22, // 22H：指纹模板非空
    TemplateEmpty        = 0x23, // 23H：指纹模板为空
    LibraryEmpty         = 0x24, // 24H：指纹库为空
    EntryCountError      = 0x25, // 25H：录入次数设置错误
    Timeout              = 0x26, // 26H：超时
    FingerExists         = 0x27, // 27H：指纹已存在
    FeatureAssociated    = 0x28, // 28H：指纹特征有关联
    SensorOperationFail  = 0x29, // 29H：传感器操作失败
    ModuleInfoNotEmpty   = 0x2A, // 2AH：模组信息非空
    ModuleInfoEmpty      = 0x2B, // 2BH：模组信息为空
    OtpOperationFail     = 0x2C, // 2CH：OTP 操作失败
    KeyGenerationFail    = 0x2D, // 2DH：秘钥生成失败
    KeyNotExist          = 0x2E, // 2EH：秘钥不存在
    AlgorithmFail        = 0x2F, // 2FH：安全算法执行失败
    AlgorithmResultError = 0x30, // 30H：安全算法加解密结果有误
    FunctionNotMatch     = 0x31, // 31H：功能与加密等级不匹配
    KeyLocked            = 0x32, // 32H：秘钥已锁定
    SmallImageArea       = 0x33, // 33H：图像面积小
    UnusableImage        = 0x34, // 34H：图像不可用
    IllegalData          = 0x35, // 35H：非法数据
    Reserved             = 0x36  // 36H：Reserve
};
struct __attribute__((packed)) finger_ACK_package {
    struct __attribute__((packed)) {
        be_uint16_t SOF;
        be_uint32_t address;
        uint8_t ID;              // ACK=0x07
        be_uint16_t length;
    } header;
    finger_status status;
    uint8_t data[128] = {};      // 末尾加上校验和
    void set_zero() { std::memset(this, 0, sizeof(finger_ACK_package)); }
};

enum class LED_modes : uint8_t {
    Breath = 1,                  // 呼吸灯
    Blink,                       // 闪烁
    AlwaysOn,                    // 常亮
    AlwaysOff,                   // 常灭
    GradualOn,                   // 渐亮
    GradualOff,                  // 渐灭
};
enum class LED_colors : uint8_t {
    Blue      = 1,               // 蓝色    001
    Green     = 2,               // 绿色    010
    Red       = 4,               // 红色    100
    RedGreen  = 6,               // 红绿双色 110
    RedBlue   = 5,               // 红蓝双色 101
    GreenBlue = 3,               // 绿蓝双色 011
    White     = 7,               // 白色    111
    None      = 0,               // 全灭    000
};
struct __attribute__((packed)) finger_led_params {
    LED_modes mode;
    LED_colors start_color;
    LED_colors end_color;
    uint8_t loop_times;          // 0表示无限循环
    finger_led_params() {
        mode        = LED_modes::Breath;
        start_color = LED_colors::GreenBlue;
        end_color   = LED_colors::GreenBlue;
        loop_times  = 0;
    }
    finger_led_params& set_mode(LED_modes mode) {
        this->mode = mode;
        return *this;
    }
    finger_led_params& set_start_color(LED_colors start_color) {
        this->start_color = start_color;
        return *this;
    }
    finger_led_params& set_end_color(LED_colors end_color) {
        this->end_color = end_color;
        return *this;
    }
    finger_led_params& set_loop_times(uint8_t loop_times) {
        this->loop_times = loop_times;
        return *this;
    }
};

struct __attribute__((packed)) finger_auto_enroll_params {
    be_uint16_t ID = 0;
    uint8_t times  = 5;

    uint8_t unused1 = 0;
    bool led_auto_off : 1 = true; // 釆图背光灯控制位，0-LED 长亮，1-LED 获取图像成功后灭；
    bool pre_process : 1 = true; // 釆图预处理控制位，0-关闭预处理，1-打开预处理；
    bool no_return_states : 1 =
        false; // 注册过程中，是否要求模组在关键步骤，返回当前状态，0-要求返回，1-不要求返回；
    bool allow_ID_cover : 1 = false; // 是否允许覆盖ID 号，0-不允许，1-允许；
    bool no_duplicate   : 1 = true; // 允许指纹重复注册控制位，0-允许，1-不允许；
    bool no_leave_after_enroll : 1 =
        false; // 注册时，多次指纹采集过程中，是否要求手指离开才能进入下一次指纹图像采集，0-要求离开；1-不要求离开；
    uint8_t unused : 2 = 0;

    finger_auto_enroll_params& set_ID(be_uint16_t ID) {
        this->ID = ID;
        return *this;
    }
    finger_auto_enroll_params& set_times(uint8_t times) {
        this->times = times;
        return *this;
    }
    finger_auto_enroll_params& set_no_leave_after_enroll(bool no_leave_after_enroll) {
        this->no_leave_after_enroll = no_leave_after_enroll;
        return *this;
    }
    finger_auto_enroll_params& set_no_duplicate(bool no_duplicate) {
        this->no_duplicate = no_duplicate;
        return *this;
    }
    finger_auto_enroll_params& set_allow_ID_cover(bool allow_ID_cover) {
        this->allow_ID_cover = allow_ID_cover;
        return *this;
    }
    finger_auto_enroll_params& set_no_return_states(bool no_return_states) {
        this->no_return_states = no_return_states;
        return *this;
    }
    finger_auto_enroll_params& set_pre_process(bool pre_process) {
        this->pre_process = pre_process;
        return *this;
    }
    finger_auto_enroll_params& set_led_always_on(bool led_always_on) {
        this->led_auto_off = led_always_on;
        return *this;
    }
};

struct __attribute__((packed)) finger_auto_identify_params {
    uint8_t score_threshold = 20; // 1~28
    be_uint16_t ID          = 0xFFFF;
    uint8_t unused1         = 0;
    bool led_auto_off : 1 = true; // 釆图背光灯控制位，0-LED 长亮，1-LED 获取图像成功后灭；
    bool pre_process : 1 = true; // 釆图预处理控制位，0-关闭预处理，1-打开预处理；
    bool no_return_states : 1 =
        false; // 注册过程中，是否要求模组在关键步骤，返回当前状态，0-要求返回，1-不要求返回；
    uint8_t unused : 5 = 0;
    finger_auto_identify_params& set_score_threshold(uint8_t score_threshold) {
        this->score_threshold = score_threshold;
        return *this;
    }
    finger_auto_identify_params& set_ID(be_uint16_t ID) {
        this->ID = ID;
        return *this;
    }
    finger_auto_identify_params& set_no_return_states(bool no_return_states) {
        this->no_return_states = no_return_states;
        return *this;
    }
    finger_auto_identify_params& set_pre_process(bool pre_process) {
        this->pre_process = pre_process;
        return *this;
    }
    finger_auto_identify_params& set_led_always_on(bool led_always_on) {
        this->led_auto_off = led_always_on;
        return *this;
    }
};
} // namespace device