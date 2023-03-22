#define NO_COMMAND 0x0
#define RING_COMMAND 0x1
#define PLAINTEXT_COMMAND 0x2
#define DISCONNECT_COMMAND 0x3
#define RING_DISMISS_COMMAND 0x4

typedef struct __attribute__ ((__packed__)) {
    uint16_t phone_states;
    uint8_t data_lengths;
    uint8_t reserved;
    uint32_t data[4][3];
    uint8_t phone_digits[4];
} HOST_PACKET;

typedef struct __attribute__ ((__packed__)) {
    uint32_t data[4][3]; // I shuffled this here to 32 bit align it
    uint8_t data_lengths;
    uint8_t phone_commands[4];
} DEVICE_PACKET;

enum usb_phone_state {
    phone_idle = 0,
    phone_dial,
    phone_traffic,
    phone_ring,
    phone_awaiting_remote_ring,
    phone_unreachable,
    phone_requesting_ring,
    phone_transitioning
};