
#include <stdio.h>
#include <stdlib.h>

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "load_calculator.h"



u_int32_t work_start = 0, work_end = 0, non_work_counter = 0, work_counter = 0;
bool doing_work;

void scale_counters() {
    if (non_work_counter > 0xAFFF || work_counter > 0xAFFF) {
        work_counter >>= 8;
        non_work_counter >>= 8;
    }
}

void load_counter_start_work() {
    work_start = time_us_32();
    non_work_counter += work_start - work_end;
    doing_work = true;
    scale_counters();
}

void load_counter_stop_work() {
    if (doing_work) {
        doing_work = false;
        work_end = time_us_32();
        work_counter += work_end - work_start;
        scale_counters();
    }
}

u_int8_t get_load() {
    u_int64_t total = non_work_counter + work_counter;
    return (u_int64_t)work_counter * 1000 / total;
}