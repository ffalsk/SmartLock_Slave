#include "bsp/dwt/dwt.h"
#include "device/can_comm/can_comm.hpp"
#include "device/face/face.hpp"
#include "device/finger/finger.hpp"
#include "main.h"

namespace app {
using namespace device;
finger finger{device::finger::finger_params()};
face face{device::face::face_params()};
can_comm can_comm{device::can_comm::can_comm_params()};

extern "C" [[noreturn]] void app_main() {
    can_comm_instance = &can_comm;
    HAL_TIM_Base_Start_IT(&htim4);
    DWT_Init();
    DWT_Delay(0.3);
    finger.Begin();
    face.Begin();
    can_comm.Begin();
    face.bind_finger(&finger);
    __enable_irq();

    DWT_Delay(0.5);                        // wait for the system to be ready

    finger.get_user_count();
    DWT_Delay(0.1);                        // wait for response

    HAL_GPIO_WritePin(
        GPIOB, GPIO_PIN_15, GPIO_PIN_SET); // when the system is ready, turn off status LED

    // finger.delete_all();
    // face.delete_all();

    finger.resume_LED();

    while (true) {
        // handle finger&face
        if (finger.is_waiting_identify()) {
            finger.identify();
        }
        if (finger.is_waiting_decode()) {
            finger.decode();
        }

        if (face.is_waiting_identify()) {
            face.identify();
        }
        if (face.is_waiting_decode()) {
            face.decode();
        }

        // success handle
        if (identify_success == true) {
            can_comm.send_identify_success();
            can_comm.send_request(request::success_tone);
            finger.set_notice(finger::LED_states::success);
            DWT_Delay(3);
            identify_success = false;
        }

        // enroll handle
        if (can_comm.get_finger_enroll_flag() == true) {
            finger.auto_enroll(finger_auto_enroll_params());
        }
        if (can_comm.get_face_enroll_flag() == true) {
            face.enroll_interactive();
        }

        // finger LED control
        if (human_detected == true) {
            if (can_comm.get_finger_day_flag() == true) {
                finger.set_LED_to_day();
            } else {
                finger.set_LED_to_night();
            }
        } else {
            finger.set_LED_off();
        }
    }
}
} // namespace app
