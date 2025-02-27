#pragma once
#include "device/can_comm/can_comm.hpp"

namespace app {
bool identify_success               = false;
bool human_detected                 = true;
device::can_comm* can_comm_instance = nullptr;

} // namespace app