/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdlib.h>

#include "pico/stdlib.h"
#include "pico/util/queue.h"
#include "hardware/pio.h"

#include "differential_manchester.pio.h"

#include "process_phones.h"
#include "recording.h"
#include "rickroll.h"
#include "load_calculator.h"


const uint LED_PIN = PICO_DEFAULT_LED_PIN;

// const u_int8_t FORCE_CLEAR = ?;
// const u_int8_t IDLE = ?;

// receiving 24

u_int8_t codewords[] = {
    0,
    //3, forces out of cue
    5,
    9,
    15,
    17,
    23,
    27,
    29,
    39,
    43,
    45,
    51,
    53,
    //63, - cue
    85,
    95,
    111,
    119,
    255
};

u_int32_t used_cycles, unused_cycles;

struct PHONE phones[] = {
    {
        .pio = pio0,
        .sm_tx = 0,
        .sm_rx = 1,
        .activity_led_pin = 0,
        .call_status_led_pin = 1,
        .pin_tx = 17,
        .pin_rx = 16,
        .phone_state = idle
    },
    {
        .pio = pio0,
        .sm_tx = 2,
        .sm_rx = 3,
        .activity_led_pin = 2,
        .call_status_led_pin = 3,
        .pin_tx = 19,
        .pin_rx = 18,
        .phone_state = idle
    },
    {
        .pio = pio1,
        .sm_tx = 0,
        .sm_rx = 1,
        .activity_led_pin = 4,
        .call_status_led_pin = 5,
        .pin_tx = 21,
        .pin_rx = 20,
        .phone_state = idle
    },
    {
        .pio = pio1,
        .sm_tx = 2,
        .sm_rx = 3,
        .activity_led_pin = 6,
        .call_status_led_pin = 7,
        .pin_tx = 27,
        .pin_rx = 26,
        .phone_state = idle
    },
}; 

struct CONNECTION connections[] = {
    {    
        .rx_word = 0,
        .has_data = false,
        .active = false,
        .requested = false,
        .associated_device = 255
    },
    {    
        .rx_word = 0,
        .has_data = false,
        .active = false,
        .requested = false,
        .associated_device = 255
    },
    {    
        .rx_word = 0,
        .has_data = false,
        .active = false,
        .requested = false,
        .associated_device = 255
    },
    {    
        .rx_word = 0,
        .has_data = false,
        .active = false,
        .requested = false,
        .associated_device = 255
    }
};


bool match_codeword(u_int32_t frame, u_int8_t codeword) {
    // rx state machine is fucked
    u_int8_t cw_to_match = ~ codeword;
    for (int i = 0; i < 8; i++) {
        // shift i bits left, bitwise and. If the XOR is 0, all bits are the same
        if ( (((frame >> i) & 0xFF ) ^ cw_to_match) == 0) {
            // if we match, try the next byte
            if ( (((frame >> (i + 8)) & 0xFF ) ^ cw_to_match) == 0) {
                if ( (((frame >> (i + 16)) & 0xFF ) ^ cw_to_match) == 0) {
                    return true;
                }
            }
        }
    }
    return false;
}

u_int32_t create_codeword_word(u_int8_t cw_rob) {
    u_int8_t codeword = ~cw_rob;
    return codeword | codeword << 8 | codeword << 16 | codeword << 24;
}

char determine_digit(u_int32_t rx_word) {
    if (match_codeword(rx_word, DIGIT_0)) {
        return '0';
    }
    if (match_codeword(rx_word, DIGIT_1)) {
        return '1';
    }
    if (match_codeword(rx_word, DIGIT_2)) {
        return '2';
    }
    if (match_codeword(rx_word, DIGIT_3)) {
        return '3';
    }
    if (match_codeword(rx_word, DIGIT_4)) {
        return '4';
    }
    if (match_codeword(rx_word, DIGIT_5)) {
        return '5';
    }
    if (match_codeword(rx_word, DIGIT_6)) {
        return '6';
    }
    if (match_codeword(rx_word, DIGIT_7)) {
        return '7';
    }
    if (match_codeword(rx_word, DIGIT_8)) {
        return '8';
    }
    if (match_codeword(rx_word, DIGIT_9)) {
        return '9';
    }
    if (match_codeword(rx_word, DIGIT_R)) {
        return 'R';
    }
    if (match_codeword(rx_word, DIGIT_C)) {
        return 'C';
    }
    if (match_codeword(rx_word, DIGIT_P)) {
        return 'P';
    }
    if (match_codeword(rx_word, DIGIT_I)) {
        return 'I';
    }
    if (match_codeword(rx_word, DIGIT_F)) {
        return 'F';
    }
    if (match_codeword(rx_word, DIGIT_FO)) {
        return 'O';
    }
    return ' ';
}



void serial_task() {
    return;
    /*
    if (phone->phone_type == serial_console) {
    //get_serial_command(remote_command);
    if (connection->requested && !connection->active) {
        printf("serial console alive\n");
        connection->active = true;
        remote_connection->active = true;
    }
    if (connection->active) {
        connection->has_data = true;
        connection->rx_word = NULL_AUDIO;
        if (remote_connection->has_data) {
            printf("p%dd%08x\n", connected_phone, remote_connection->rx_word);
            remote_connection->has_data = false;
        }
    }
}*/
}


void phone_task() {
    char digit;
    if (used_cycles > 0xFFF || unused_cycles > 0xFFF) {
        used_cycles >>= 8;
        unused_cycles >>= 8;
    }
    for (int i = 0; i < NUMBER_OF_PHONES; i++) {
        u_int64_t current_time = time_us_64();
        u_int32_t rx_word;
        struct PHONE *phone = phones + i;
        struct CONNECTION *connection = connections + i;
        u_int8_t connected_phone = connection->associated_device;
        struct CONNECTION *remote_connection = connections + connected_phone;

        // if we're in idle but a call is requested, move to ring
        if  (phone->phone_state == idle && connection->requested) {
            printf("phone %d received request, push ring command\n", i);
            phone->phone_state = requesting_ring;
            // initial ring requires pushing command without rx until ring ack received
            phone->pushing_command = true;
            phone->attempted_contact = current_time;
        }
        /*
        if (phone->phone_state == ringing && !connection->requested) {
            printf("phone %d ring cancel begin, push cue\n", i);
            phone->phone_state = ring_dismiss_send_cue;
            phone->pushing_command = true;
        }*/
        bool tx_fifo_full = pio_sm_is_tx_fifo_full(phone->pio, phone->sm_tx),
        rx_fifo_empty = pio_sm_is_rx_fifo_empty(phone->pio, phone->sm_rx);
        if (!phone->idle_data_cleared && phone->phone_state == idle && (current_time - phone->last_data_received_time > 1000000)) {
            printf("phone %d: idle for one second, clearing pio\n", i);
            gpio_put(phone->activity_led_pin, 0);
            gpio_put(phone->call_status_led_pin, 0);
            phone->idle_data_cleared = true;
            pio_sm_restart(phone->pio, phone->sm_rx);
        }
        // phone not in idle, not pushing command, and > 0.5s since last data, send to idle
        if (phone->phone_state != idle && !phone->pushing_command && (current_time - phone->last_data_received_time > 5000000)) {
            printf("phone %d: no rx in .5 seconds, entering idle\n", i);
            phone->phone_state = idle;
        }
        if (phone->pushing_command) {
            if (tx_fifo_full) {
                // if our fifo is full we will block on push, therefore we should wait
                unused_cycles++;
                continue;
            }
            if (rx_fifo_empty) {
                // we set this to null audio and continue
                rx_word = NULL_AUDIO;
                load_counter_start_work();
            } else {
                // if we received new data, phone is "alive"
                rx_word = pio_sm_get_blocking(phone->pio, phone->sm_rx);
                load_counter_start_work();
                phone->last_data_received_time = current_time;
                phone->activity_counter++;
                phone->idle_data_cleared = false;
            }
            
        } else {
            // normal mode, expect back and fourth
            if (rx_fifo_empty) {
                // no data available for this phone, skip it
                unused_cycles++;
                continue;
            } else {
                used_cycles++;
                load_counter_start_work();
                rx_word = pio_sm_get_blocking(phone->pio, phone->sm_rx);
                phone->activity_counter++;
                phone->last_data_received_time = current_time;
                phone->idle_data_cleared = false;
            }
        }
        if (phone->activity_counter > 128) {
            gpio_put(phone->activity_led_pin, 1);
        } else {
            gpio_put(phone->activity_led_pin, 0);
        }
        // we expect release during transition to idle
        if (match_codeword(rx_word, RELEASE) && phone->phone_state != transition_to_idle) {
            printf("phone %d release rx, transition to idle\n", i);
            phone->phone_state = transition_to_idle;
            if (connection->requested || connection->active) {
                connection->requested = false;
                connection->active = false;
                gpio_put(phone->call_status_led_pin, 0);
                connections[connected_phone].requested = false;
                connections[connected_phone].active = false;
                printf("phone %d clearing connected phone %d\n", i, connected_phone);
                /*
                printf(" connection0 req %d, connection1 req %d, ", connections[0].requested, connections[1].requested);
                printf("connected phone %d []requested %d active %d\n", connected_phone, connections[connected_phone].requested, connections[connected_phone].active);
                */
            }
            phone->sent_codewords = 0;
            phone->pushing_command = true;
        }

        switch (phone->phone_state) {
            case idle:
                if (match_codeword(rx_word, SEIZE)) {
                    phone->phone_state = transition_to_dial;
                    phone->sent_codewords = 0;
                    printf("phone %d transition to dial, matched %02x with %08x\n", i, SEIZE, rx_word);
                }
                break;
            case transition_to_dial:
                if (match_codeword(rx_word, INTERDIGIT)) {
                    phone->phone_state = receiving_dial;
                    printf("phone %d entering dial mode\n", i);
                    phone->digit_count = 0;
                    phone->current_digit = ' ';
                }
                pio_sm_put_blocking(phone->pio, phone->sm_tx, create_codeword_word(DIAL));
                break;
            case cue_transition_to_dial:
                if (match_codeword(rx_word, SEIZE)) {
                    phone->phone_state = transition_to_dial;
                    printf("phone %d sent cue, received seize, go to regular dial\n", i);
                }
                pio_sm_put_blocking(phone->pio, phone->sm_tx, create_codeword_word(CUE));
                break;
            case cue_transition_to_traffic_dial:
                if (match_codeword(rx_word, SEIZE)) {
                    phone->phone_state = transition_to_traffic_dial;
                    printf("phone %d sent cue, received seize, go to traffic dial\n", i);
                }
                pio_sm_put_blocking(phone->pio, phone->sm_tx, create_codeword_word(CUE));
                break;
            case transition_to_traffic_dial:
                if (match_codeword(rx_word, INTERDIGIT)) {
                    phone->phone_state = traffic_dial;
                    printf("phone %d entering traffic dial mode\n", i);
                }
                pio_sm_put_blocking(phone->pio, phone->sm_tx, create_codeword_word(DIAL));
                break;
            case traffic_dial:
                if (!connection->active) {
                    // phone already in dial, so if it dies go straight to dial
                    phone->phone_state = receiving_dial;
                    phone->digit_count = 0;
                    phone->current_digit = ' ';
                    continue;
                }
                if (phone->current_digit == 'C' && phone->receiving_digit == false) {
                    phone->phone_state = transition_to_plaintext;
                    printf(" - phone %d dialing complete, transition to ring\n", i);
                    phone->current_digit = ' ';
                }
                if (phone->receiving_digit) {
                    if (match_codeword(rx_word, INTERDIGIT)) {
                        printf("%c", phone->current_digit);
                        phone->receiving_digit = false;
                        phone->digit_count++;
                    }
                } else {
                    digit = determine_digit(rx_word);
                    if (digit != ' ') {
                        phone->current_digit = digit;
                        phone->receiving_digit = true;
                    }
                }
                connection->rx_word = NULL_AUDIO;
                connection->has_data = true;
                if (connections[connected_phone].has_data) {
                    pio_sm_put_blocking(phone->pio, phone->sm_tx, connections[connected_phone].rx_word);
                    connections[connected_phone].has_data = false;
                }
                break;
            case cue_until_sieze:
                if (match_codeword(rx_word, SEIZE)) {
                    phone->phone_state = transition_to_dial;
                    printf("phone %d sent cue, received seize, go to dial\n", i);
                }
                pio_sm_put_blocking(phone->pio, phone->sm_tx, create_codeword_word(CUE));
                break;
            /*case test_transition_codewords:
                if (transmitted_test_codewords >= 4000) {
                    transmitted_test_codewords = 0;
                    if (test_codeword_index >= number_codewords - 1) {
                        test_codeword_index = 0;
                    } else {
                        test_codeword_index++;
                    }
                    printf("\n** %d testing %02x - \n", i, codewords[test_codeword_index], rx_word);
                }
                printf("%08x", ~rx_word);
                pio_sm_put_blocking(phone->pio, phone->sm_tx, create_codeword_word(codewords[test_codeword_index]));
                transmitted_test_codewords++;
                break;*/
            case receiving_dial:
                if (phone->current_digit == 'R' && phone->receiving_digit == false) {
                    printf("phone %d go to rickroll\n", i);
                    phone->phone_state = rickroll;
                    phone->recording_index = 0;
                    phone->current_digit = ' ';
                    continue;
                }
                if (phone->current_digit == 'C' && phone->receiving_digit == false) {
                    u_int8_t target_device;
                    if (phone->digits[0] == '1'){
                        target_device = 0;
                    } else if (phone->digits[0] == '2') {
                        target_device = 1;
                    } else if (phone->digits[0] == '3') {
                        target_device = 2;
                    /*} else if (phone->digits[0] == 'C') {
                        phone->phone_state = conference;
                        phone->current_digit = ' ';
                        printf(" - %d dialing complete, going to conference mode\n", i);*/
                    } else if (phone->digits[0] == '4') {
                        target_device = 3;
                    } else {
                        target_device = 255;
                    }
                    phone->current_digit = ' ';
                    if (target_device != 255) {
                        // phone number 1 to device 0
                        connection->associated_device = target_device;
                        connection->requested = true;
                        phone->digits[phone->digit_count] = '\0';
                        // should trigger ring
                        connections[target_device].associated_device = i;
                        connections[target_device].requested = true;
                        phone->phone_state = awaiting_remote_ring;
                        printf(" - %d dialing complete, received %s connecting to %d\n", i, phone->digits, connection->associated_device);
                    } else {
                        phone->phone_state = not_in_service_recording;
                        phone->recording_index = 0;
                    }
                }
                if (phone->receiving_digit) {
                    if (match_codeword(rx_word, INTERDIGIT)) {
                        printf("%c", phone->current_digit);
                        phone->digits[phone->digit_count] = phone->current_digit;
                        phone->receiving_digit = false;
                        phone->digit_count++;
                    }
                }
                digit = determine_digit(rx_word);
                if (digit != ' ') {
                    phone->current_digit = digit;
                    phone->receiving_digit = true;
                }
                pio_sm_put_blocking(phone->pio, phone->sm_tx, 0xffeb14aa);
                break;
            case not_in_service_recording:
                if (phone->recording_index > 5122) {
                    phone->phone_state = connection_failure;
                }
                pio_sm_put_blocking(phone->pio, phone->sm_tx, recording[phone->recording_index++]);
                break;
            case rickroll:
                if (phone->recording_index > 23338) {
                    phone->phone_state = receiving_dial;
                }
                pio_sm_put_blocking(phone->pio, phone->sm_tx, rickroll_recording[phone->recording_index++]);
                break;
            case connection_failure:
                if (phone->receiving_digit) {
                    if (match_codeword(rx_word, INTERDIGIT)) {
                        printf("phone %d received key, back to dial: %c\n", i, phone->current_digit);
                        phone->receiving_digit = false;
                        phone->digit_count = 0;
                        phone->phone_state = receiving_dial;
                        phone->current_digit = ' ';
                    }
                }
                digit = determine_digit(rx_word);
                if (digit != ' ') {
                    phone->current_digit = digit;
                    phone->receiving_digit = true;
                }
                if (phone->activity_counter < 50) {
                    pio_sm_put_blocking(phone->pio, phone->sm_tx, 0xffeb14aa);
                } else {
                    pio_sm_put_blocking(phone->pio, phone->sm_tx, NULL_AUDIO);
                }
                break;
            case awaiting_remote_ring:
                if (!connection->requested) {
                    phone->phone_state = connection_failure;
                }
                if (connection->active) {
                    phone->phone_state = transition_to_plaintext;
                }
                // TODO ring tone
                pio_sm_put_blocking(phone->pio, phone->sm_tx, NULL_AUDIO);
                break;
            case requesting_ring:
                if (match_codeword(rx_word, RING_ACK)) {
                    phone->phone_state = ringing;
                    printf("%d received ring ack, she's alive, going to ringing state\n", i);
                    phone->sent_codewords = 0;
                    phone->pushing_command = false;
                }
                // we loitered here for half second without ack, assume line dead
                if (current_time - phone->attempted_contact > 5000000) {
                    connection->requested = false;
                    connections[connected_phone].requested = false;
                    phone->phone_state = idle;
                }
                pio_sm_put_blocking(phone->pio, phone->sm_tx, create_codeword_word(RING_VOICE));
                break;
            case ringing:
                if (match_codeword(rx_word, RING_TRIP)) {
                    phone->phone_state = transition_to_plaintext;
                    connection->active = true;
                    connections[connected_phone].active = true;
                    printf("phone %d: received ring trip, transition_to_plaintext\n", i);
                    gpio_put(phone->call_status_led_pin, 1);
                }
                if (!connection->requested) {
                    printf("phone %d: other connetion dropped, ring dismiss flow\n", i);
                    phone->phone_state = ring_dismiss_send_cue;
                    gpio_put(phone->call_status_led_pin, 0);
                }
                if (phone->activity_counter > 128) {
                    gpio_put(phone->call_status_led_pin, 1);
                } else {
                    gpio_put(phone->call_status_led_pin, 0);
                }
                pio_sm_put_blocking(phone->pio, phone->sm_tx, NULL_AUDIO);
                break;
            case transition_to_plaintext:
                if (match_codeword(rx_word, LOCK_IN)) {
                    phone->phone_state = acknowledge_lock_in;
                    printf("phone %d: received lock in request, acknowedge_lock_in\n", i);
                    phone->sent_codewords = 0;
                }
                pio_sm_put_blocking(phone->pio, phone->sm_tx, create_codeword_word(GO_TO_PLAIN_TEXT));
                break;
            case acknowledge_lock_in:
                if (phone->sent_codewords > 50) {
                    phone->phone_state = plain_text;
                    printf("transition to plaintext\n");
                } else {
                    phone->sent_codewords++;
                }
                pio_sm_put_blocking(phone->pio, phone->sm_tx, create_codeword_word(LOCK_IN_ACK));
                break;
            case transition_to_idle:
                if (phone->sent_codewords > 50) {
                    phone->phone_state = idle;
                    printf("phone %d codewords sent, entering idle\n", i);
                    phone->pushing_command = false;
                    // this should clear the ISR of partial RX
                    pio_sm_restart(phone->pio, phone->sm_rx);
                } else {
                    phone->sent_codewords++;
                    pio_sm_put_blocking(phone->pio, phone->sm_tx, create_codeword_word(RELEASE_ACK));
                }
                break;
            case plain_text:
                if (!connection->active) {
                    phone->phone_state = cue_until_sieze;
                    gpio_put(phone->call_status_led_pin, 0);
                    continue;
                }
                if (match_codeword(rx_word, DIGIT_C)) {
                    printf("phone %d matched C during plaintext, go to cue\n", i);
                    phone->phone_state = cue_transition_to_traffic_dial;
                    continue;
                }
                if (match_codeword(rx_word, DIGIT_R)) {
                    printf("phone %d matched R during plaintext, go to regular dial\n", i);
                    phone->phone_state = cue_transition_to_dial;
                    connection->active = false;
                    connection->requested = false;
                    remote_connection->active = false;
                    remote_connection->requested = false;
                    continue;
                }
                gpio_put(phone->call_status_led_pin, 1);
                connection->rx_word = rx_word;
                connection->has_data = true;
                if (connections[connected_phone].has_data) {
                    pio_sm_put_blocking(phone->pio, phone->sm_tx, connections[connected_phone].rx_word);
                    connections[connected_phone].has_data = false;
                }
                break;
            case ring_dismiss_send_cue:
                if (match_codeword(rx_word, CUE_RELEASE_ACK)) {
                    phone->phone_state = ring_dismiss_send_dial;
                    printf("phone %d ring dismiss, send cue to send dial\n", i);
                    phone->pushing_command = false;
                }
                pio_sm_put_blocking(phone->pio, phone->sm_tx, create_codeword_word(CUE));
                break;
            case ring_dismiss_send_dial:
                if(match_codeword(rx_word, RELEASE)) {
                    printf("phone %d ring dismiss, received release now transition to idle\n", i);
                    phone->phone_state = transition_to_idle;
                    phone->pushing_command = true;
                    phone->sent_codewords = 0;
                }
                pio_sm_put_blocking(phone->pio, phone->sm_tx, create_codeword_word(DIAL));
                break;
            default:
                break;
        }
        load_counter_stop_work();
        /*for (int i = 0; i < 100; i++) {
            pio_sm_put_blocking(pio, sm_tx, 0x00000000);
            pio_sm_put_blocking(pio, sm_tx, 0xffffffff);
        }
        for (int i = 0; i < 5000 ; i++) {
            recording[i] = pio_sm_get_blocking(pio, sm_rx);
        }
        for (int i = 0; i < 100; i++) {
            pio_sm_put_blocking(pio, sm_tx, 0x00000000);
            pio_sm_put_blocking(pio, sm_tx, 0xffffffff);
        }
        for (int i = 0; i < 5000; i++) {
            pio_sm_put_blocking(pio, sm_tx, recording[i]);
        }*/
    }
}

void init_phones() {

    uint pio0_offset_tx = pio_add_program(pio0, &differential_manchester_tx_program);
    uint pio0_offset_rx = pio_add_program(pio0, &differential_manchester_rx_program);
    uint pio1_offset_tx = pio_add_program(pio1, &differential_manchester_tx_program);
    uint pio1_offset_rx = pio_add_program(pio1, &differential_manchester_rx_program);
    printf("Transmit program loaded at %d\n", pio0_offset_tx);
    printf("Receive program loaded at %d\n", pio0_offset_rx);
    
    for (int i = 0; i < NUMBER_OF_PHONES; i++) {
        uint offset_tx, offset_rx;
        if (phones[i].pio == pio0) {
            offset_rx = pio0_offset_rx;
            offset_tx = pio0_offset_tx;
        } else {
            offset_rx = pio1_offset_rx;
            offset_tx = pio1_offset_tx;
        }
        gpio_init(phones[i].activity_led_pin);
        gpio_set_dir(phones[i].activity_led_pin, GPIO_OUT);
        gpio_init(phones[i].call_status_led_pin);
        gpio_set_dir(phones[i].call_status_led_pin, GPIO_OUT);
        // 16 cylces/symbol, 32 khz total, 125mhz base freq in khz
        differential_manchester_tx_program_init(phones[i].pio, phones[i].sm_tx, offset_tx, phones[i].pin_tx, 125000.f / (32 * 16));
        differential_manchester_rx_program_init(phones[i].pio, phones[i].sm_rx, offset_rx, phones[i].pin_rx, 125000.f / (32 * 16));
    }

    //uint32_t* recording = malloc(5000 * sizeof(uint32_t));

}
