#include "pico/stdlib.h"

#ifndef DNVT_SWITCH_H
#define DNVT_SWITCH_H

#define DIP0 15
#define DIP1 14
#define DIP2 13
#define DIP3 28

u_int8_t get_dip_value(int);

#define DIP_DISABLE_SUPERVISORY 0
#define DIP_DISABLE_LINE_SIMULATOR 1

#define RESET_BUTTON 4

#endif