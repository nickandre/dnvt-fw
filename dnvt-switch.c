#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "pico/multicore.h"

#include "display.h"
#include "process_phones.h"
#include "dev_lowlevel.h"
#include "dnvt-switch.h"

u_int32_t dips[] = {DIP0, DIP1, DIP2, DIP3};

void core1_entry() {
    while(1) {
        update_display();
        sleep_ms(250);
    }
}

int init_dips() {
    gpio_init(RESET_BUTTON);
    gpio_set_pulls(RESET_BUTTON, true /*up*/, false /*down*/);
    gpio_set_dir(RESET_BUTTON, GPIO_IN);
    for (int i = 0; i < sizeof(dips)/sizeof(dips[0]); i++) {
        gpio_init(dips[i]);
        gpio_set_pulls(dips[i], true /*up*/, false /*down*/);
        gpio_set_dir(dips[i], GPIO_IN);
    }
}

void check_reset() {
    if (!gpio_get(RESET_BUTTON) || fw_update_usb_request)  {
        display_fw_update();
        reset_usb_boot(0,0);
    }
}


u_int8_t dip_values[4];
void get_dips() {
    // "ON" for each DIP means 
    for (int i = 0; i < sizeof(dips)/sizeof(dips[0]); i++) {
        dip_values[i] = gpio_get(dips[i]);
    }
}

u_int8_t get_dip_value(int index) {
    return dip_values[index];
}


int main()
{
    stdout_uart_init();

    init_phones();
    init_display();
    multicore_launch_core1(core1_entry);
    init_dips();


    while(1) {
        usb_housekeeping();
        phone_task();
        check_reset();
        get_dips();
    }
}
