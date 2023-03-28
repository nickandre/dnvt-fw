#include <stdlib.h>

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "pico/util/queue.h"

#define GAIN_STEP 0xB5// 0.71 = 0x0.B5C2, do 8 bit shift
#define GAIN_DECAY 0xFCD1 // 0.9875778 = 0x0.FCD1, 16 bit shift
#define SIGNAL_DECAY 0xFAE1 // 0.98 = 0.FAE1
#define AUDIO_SCALE 250
#define NUMBER_OF_PHONES 4

#define CUE 63
#define CUE_RELEASE_ACK 111
#define SEIZE 111 // rx 0x90 = 10010000, rob 01101111
#define DIAL 53 // 1100 1010 rob 0011 0101 
#define INTERDIGIT 85 // rob 01010101
#define GO_TO_PLAIN_TEXT 3 // rob 00000011
#define LOCK_IN 0
#define LOCK_IN_ACK 0
#define RELEASE 95 // rx 1010 0000, rob 01011111
#define RELEASE_ACK 43 // tx 1011 1100, rob 01000011
#define RING_VOICE 45 // 00101101
#define RING_ACK 53
#define RING_TRIP 43
#define DIGIT_0 5 // rob 0000 0101
#define DIGIT_1 15
#define DIGIT_2 39 // rob 0010 0111
#define DIGIT_3 63 // rob 0011 1111
#define DIGIT_4	29
#define DIGIT_5	3
#define DIGIT_6	43
#define DIGIT_7	119
#define DIGIT_8	141
#define DIGIT_9	9
#define DIGIT_R	111
#define DIGIT_C 17 // Conference Request
#define DIGIT_P 23 // Priority
#define DIGIT_I 45 //Immediate
#define DIGIT_F 51 //Flash
#define DIGIT_FO 53 //flash override
//RATE CHANGE	X		Unsure
//RATE CHANGE ACK		X	Unsure
//RECALL		X	Unsure

#define NULL_AUDIO 0xAAAAAAAA

#ifndef PHONE_DATA
#define PHONE_DATA

#define number_codewords 18

enum PHONE_STATE {
    idle = 0,
    off_hook = 1,
    transition_to_dial = 2,
    receiving_dial = 3,
    acknowledge_lock_in = 4,
    transition_to_plaintext = 5,
    ringing = 6,
    plain_text = 7,
    transition_to_idle = 8,
    record_and_playback = 9,
    ringing_remote = 10,
    awaiting_remote_ring = 11,
    ring_dismiss_send_cue = 12,
    ring_dismiss_send_dial = 13,
    send_release_ack = 14,
    transition_to_traffic_dial = 15,
    cue_until_sieze = 16,
    traffic_dial = 17,
    cue_transition_to_traffic_dial = 18,
    requesting_ring = 19,
    connection_failure = 20,
    cue_transition_to_dial = 21,
    not_in_service_recording = 22,
    rickroll = 23,
    usb_dial = 24,
    usb_traffic = 25,
    unreachable,
    line_check
};

struct PHONE {
    // state machine/pin info
    PIO pio;
    u_int8_t sm_tx;
    u_int8_t sm_rx;
    u_int8_t pin_tx;
    u_int8_t pin_rx;
    // current state
    volatile enum PHONE_STATE phone_state;
    // are we pushing a command without rx?
    bool pushing_command;
    // list of received digits, current digit
    char digits[20];
    char current_digit;
    uint8_t last_transmitted_digit;
    u_int8_t digit_count;
    bool receiving_digit;
    // if sending fixed qty of codewords, count that here
    u_int32_t sent_codewords;
    // last activity received from phone in microseconds
    u_int64_t last_data_received_time;
    // attempted contact timestamp
    u_int64_t attempted_contact;
    // activity counter for status blinky blinky
    u_int8_t activity_counter;
    // have we cleared the sm?
    bool idle_data_cleared;
    u_int32_t recording_index;
    u_int8_t received_codeword_counter;
};

#define CONNECTED_TO_USB 0xFE
#define NOT_CONNECTED 0xFF

struct CONNECTION {
    queue_t rx_queue;
    queue_t tx_queue;
    bool active;
    bool requested;
    u_int8_t associated_device;
};

extern u_int32_t used_cycles, unused_cycles;
/*
LED layout
1 3 5 7 23
0 2 4 6 (3.3V)
*/
extern struct PHONE phones[];

extern struct CONNECTION connections[];

void init_phones();

void phone_task();

uint8_t create_host_packet(uint8_t*);
void handle_device_packet(uint8_t*);
#endif
