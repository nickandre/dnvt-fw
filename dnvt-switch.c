#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "pico/multicore.h"

#include "display.h"
#include "process_phones.h"
#include "dev_lowlevel.h"

#define DIP0 15
#define DIP1 14
#define DIP2 13
#define DIP3 28

#define RESET_BUTTON 4

u_int32_t current_time_32;
u_int32_t last_display_update = 0;

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
}

void check_reset() {
    if (!gpio_get(RESET_BUTTON)) {
        display_fw_update();
        reset_usb_boot(0,0);
    }
}

int get_dips() {

}


int main()
{
    stdout_uart_init();

    init_phones();
    init_display();
    multicore_launch_core1(core1_entry);
    init_dips();
    //usb_init();

    while(1) {
        current_time_32 = time_us_32();
        if (current_time_32 - last_display_update > 250 * 1000) {
            last_display_update = current_time_32;
        }
        phone_task();
        check_reset();
    }
}
