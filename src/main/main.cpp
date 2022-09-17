#ifndef DREAMCAST_CONTROLLER_USB_PICO_TEST

#include "bsp/board.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/structs/systick.h"
#include "device/dcd.h"

#include "configuration.h"
#include "hardware/structs/systick.h"
#include "hardware/regs/m0plus.h"

#include "MapleBus.hpp"
#include "DreamcastNode.hpp"
#include "DreamcastMainNode.hpp"
#include "PlayerData.hpp"
#include "CriticalSectionMutex.hpp"

#include "UsbGamepad.h"
#include "UsbGamepadDreamcastControllerObserver.hpp"
#include "usb_descriptors.h"
#include "usb_execution.h"

#define MAPLE_HOST_ADDRESS 0x00

UsbGamepad usbGamepads[NUMBER_OF_DEVICES] = {
    UsbGamepad(ITF_NUM_HID1),
    UsbGamepad(ITF_NUM_HID2),
    UsbGamepad(ITF_NUM_HID3),
    UsbGamepad(ITF_NUM_HID4)
};
UsbGamepadDreamcastControllerObserver usbGamepadDreamcastControllerObservers[NUMBER_OF_DEVICES] = {
    UsbGamepadDreamcastControllerObserver(usbGamepads[0]),
    UsbGamepadDreamcastControllerObserver(usbGamepads[1]),
    UsbGamepadDreamcastControllerObserver(usbGamepads[2]),
    UsbGamepadDreamcastControllerObserver(usbGamepads[3])
};
CriticalSectionMutex screenMutexes[NUMBER_OF_DEVICES];
ScreenData screenData[NUMBER_OF_DEVICES] = {
    ScreenData(screenMutexes[0]),
    ScreenData(screenMutexes[1]),
    ScreenData(screenMutexes[2]),
    ScreenData(screenMutexes[3])
};
PlayerData playerData[NUMBER_OF_DEVICES] = {
    {0, usbGamepadDreamcastControllerObservers[0], screenData[0]},
    {1, usbGamepadDreamcastControllerObservers[1], screenData[1]},
    {2, usbGamepadDreamcastControllerObservers[2], screenData[2]},
    {3, usbGamepadDreamcastControllerObservers[3], screenData[3]}
};
MapleBus busses[NUMBER_OF_DEVICES] = {
    MapleBus(P1_BUS_START_PIN, MAPLE_HOST_ADDRESS),
    MapleBus(P2_BUS_START_PIN, MAPLE_HOST_ADDRESS),
    MapleBus(P3_BUS_START_PIN, MAPLE_HOST_ADDRESS),
    MapleBus(P4_BUS_START_PIN, MAPLE_HOST_ADDRESS),
};
DreamcastMainNode dreamcastMainNodes[NUMBER_OF_DEVICES] = {
    DreamcastMainNode(busses[0], playerData[0], std::make_shared<PrioritizedTxScheduler>(DreamcastMainNode::MAX_PRIORITY)),
    DreamcastMainNode(busses[1], playerData[1], std::make_shared<PrioritizedTxScheduler>(DreamcastMainNode::MAX_PRIORITY)),
    DreamcastMainNode(busses[2], playerData[2], std::make_shared<PrioritizedTxScheduler>(DreamcastMainNode::MAX_PRIORITY)),
    DreamcastMainNode(busses[3], playerData[3], std::make_shared<PrioritizedTxScheduler>(DreamcastMainNode::MAX_PRIORITY))
};

UsbControllerInterface* devices[NUMBER_OF_DEVICES] = {
    &usbGamepads[0],
    &usbGamepads[1],
    &usbGamepads[2],
    &usbGamepads[3]
};

// Second Core Process
// The second core is in charge of handling communication with Dreamcast peripherals
void core1()
{
    set_sys_clock_khz(CPU_FREQ_KHZ, true);

    // Wait for steady state
    sleep_ms(100);

    while(true)
    {
        for (DreamcastMainNode* p_node = &dreamcastMainNodes[0];
             p_node <= &dreamcastMainNodes[NUMBER_OF_DEVICES - 1];
             ++p_node)
        {
            // Worst execution duration of below is ~350 us at 133 MHz when debug print is disabled
            p_node->task(time_us_64());
        }
    }
}

// First Core Process
// The first core is in charge of initialization and USB communication
int main()
{
    set_sys_clock_khz(CPU_FREQ_KHZ, true);

    board_init();

#if SHOW_DEBUG_MESSAGES
    stdio_init_all();
#endif

    multicore_launch_core1(core1);

    set_usb_devices(devices, sizeof(devices) / sizeof(devices[1]));

    usb_init();

#if USB_LED_PIN >= 0
    gpio_init(USB_LED_PIN);
    gpio_set_dir_out_masked(1<<USB_LED_PIN);
#endif

#if SIMPLE_USB_LED_PIN >= 0
    gpio_init(SIMPLE_USB_LED_PIN);
    gpio_set_dir_out_masked(1<<SIMPLE_USB_LED_PIN);
#endif

    while(true)
    {
        usb_task();
    }
}

#endif
