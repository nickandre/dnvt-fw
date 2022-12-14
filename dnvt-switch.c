#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "pico/multicore.h"

#include "display.h"
#include "process_phones.h"



u_int32_t current_time_32;
u_int32_t last_display_update = 0;

void core1_entry() {
    while(1) {
        update_display();
        sleep_ms(250);
    }
}


int main()
{
    stdio_init_all();

    init_phones();
    init_display();
    multicore_launch_core1(core1_entry);

    while(1) {
        current_time_32 = time_us_32();
        if (current_time_32 - last_display_update > 250 * 1000) {
            last_display_update = current_time_32;
        }
        phone_task();
    }
}
