#include <stdio.h>
#include <string.h>
#include "display.h"
#include "process_phones.h"
#include "load_calculator.h"


ssd1306_t disp;

void update_display(){
    char buf[30];
    ssd1306_clear(&disp);
    ssd1306_draw_string(&disp, 0, 0*LINE_SPACE, 1, "DNVT Switch ver 0.1");
    ssd1306_draw_string(&disp, 0, 1*LINE_SPACE, 1, "Ln  Status  Conn. To");
    
    for (int i = 0; i < NUM_PHONES; i++) {
        struct PHONE *phone = phones + i;
        char phone_state[9], connected_phone[10];
        switch(phone->phone_state) {
            case idle:
            case transition_to_idle:
            case ring_dismiss_send_cue:
            case ring_dismiss_send_dial:
            case send_release_ack:
                strcpy(phone_state, "idle");
                break;
            case off_hook:
            case transition_to_dial:
            case receiving_dial:
            case cue_until_sieze:
            case cue_transition_to_dial:
                strcpy(phone_state, "dial");
                break;
            case ringing:
            case requesting_ring:
                strcpy(phone_state, "ringing");
                break;
            case transition_to_plaintext:
            case acknowledge_lock_in:
            case plain_text:
                strcpy(phone_state, "traffic");
                break;
            case awaiting_remote_ring:
                strcpy(phone_state, "r ring");
                break;
            case transition_to_traffic_dial:
            case traffic_dial:
            case cue_transition_to_traffic_dial:
                strcpy(phone_state, "t dial");
                break;
            case connection_failure:
            case not_in_service_recording:
                strcpy(phone_state, "con fail");
                break;
            case rickroll:
                strcpy(phone_state, "rickroll");
                break;
            default:
                strcpy(phone_state, "---");
                break;
        }
        if (phone->phone_state == plain_text) {
            strncpy(connected_phone, phone->digits, 9);
        } else {
            strcpy(connected_phone, "n/a");
        }
        sprintf(buf, "%i %-10s%s", i + 1, phone_state, connected_phone);
        ssd1306_draw_string(&disp, 0, (2 + i)*LINE_SPACE, 1, buf);
    }
    ssd1306_draw_line(&disp, 1*CHAR_WIDTH + 2, 2*LINE_SPACE, 1*CHAR_WIDTH + 2, 6*LINE_SPACE);
    ssd1306_draw_line(&disp, 11*CHAR_WIDTH, 2*LINE_SPACE, 11*CHAR_WIDTH, 6*LINE_SPACE);
    // u_int64_t cycle_ctr = used_cycles + unused_cycles;
    // u_int8_t load = ((u_int64_t)used_cycles) * 1000 / cycle_ctr;
    sprintf(buf, "Load: %03d%%   USB: n/c", get_load());
    ssd1306_draw_string(&disp, 0, 56, 1, buf);
    ssd1306_show(&disp);
}

void init_display() {
    // I2C Initialisation. Using it at 400Khz.
    i2c_init(I2C_PORT, 400*1000);
    
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    disp.external_vcc=false;
    ssd1306_init(&disp, 128, 64, 0x3C, I2C_PORT);
}