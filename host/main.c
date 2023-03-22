#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libusb.h>
#include <err.h>
#include <ncurses.h>
#include <pthread.h>
#include <sys/queue.h>
#include <assert.h>

#include "../usb_structures.h"

#define MFGR_ID 0xCAFE // given manufacturer ID 
#define DEV_ID 0x6942  // given device ID



#define USB_CONTROL_VENDOR_MESSAGE 0x2<<4
#define DNVT_REQUEST_STATUS 0x0
#define DNVT_REBOOT_FIRMWARE 0xff
#define SETUP_INPUT 0x1 << 7

typedef struct {
    uint32_t data[20];
    uint8_t size;
} QUEUE;

enum line_state {
    line_uninitialized = 0,
    line_idle,
    line_dial,
    line_awaiting_remote_ring,
    line_request_ring,
    line_ringing,
    line_traffic,
    line_remote_hangup,
    line_busy_signal,
    line_unreachable
};

#define NOT_CONNECTED 0xFFFF


typedef struct {
    char digits[20];
    uint8_t digit_count;
    uint8_t state;
    uint8_t connected_phone;
    QUEUE rxd;
    QUEUE txd;
    uint8_t pending_command;
    uint8_t line_state;
    uint16_t connected_device;
    uint32_t recording_index;
    bool playing_recording;
    int recording_number;
    struct timespec last_tx;
} PHONE;

typedef struct {
    uint32_t *data;
    uint32_t length;
} RECORDING;

#define DIALTONE_RECORDING 0
#define BUSY_RECORDING 1
#define RINGTONE_RECORDING 2
#define RICKROLL_RECORDING 3

RECORDING recording[4];

/* If device IDs are not known, use libusb_get_device_list() to see a 
list of all USB devices connected to the machine. Follow this call with    
libusb_free_device_list() to free the allocated device list memory.
*/

HOST_PACKET host_packet;
DEVICE_PACKET device_packet;
FILE *logfile;

#define MAX_SWITCHES 4
#define MAX_PHONES 4*MAX_SWITCHES

PHONE phones[MAX_PHONES];
struct libusb_device_handle *dnvt_sw[MAX_SWITCHES];
char dev_serials[MAX_SWITCHES][20];
int open_devices = 0;

bool thread_run = true;


// apparently C doesn't have an STL queue, so...
void init_queue(QUEUE *q) {
    q->size = 0;
}

uint8_t queue_size(QUEUE *q) {
    return q->size;
}

uint32_t queue_pop(QUEUE *q) {
    assert(q->size > 0);
    uint32_t return_value = q->data[0];
    q->size--;
    for (int i = 0; i < q->size; i++) {
        q->data[i] = q->data[i+1];
    }
    return return_value;
}

bool queue_push(QUEUE *q, uint32_t data) {
    if (q->size > 20) {
        return false;
    }
    q->data[q->size++] = data;
    return true;
}

void init_phones() {
    for (int i = 0; i < MAX_PHONES; i++) {
        init_queue(&phones[i].rxd);
        init_queue(&phones[i].txd);
    }
}

int device_packets_sent = 0;
unsigned char tx_buf[64];
unsigned char rx_buf[64];
char debug[120];
uint32_t *dialtone;
uint32_t dialtone_length;


char * dialplan[] = {
    "01",
    "02",
    "03",
    "04",
    "05",
    "06",
    "07",
    "08"
};



int match_dialplan(char digits[]) {
    for (int i = 0; i < open_devices * 4; i++) {
        if(strcmp(digits, dialplan[i]) == 0) {
            return i;
        }
    }
    return -1;
}

uint32_t clock_diff_ms(struct timespec *old, struct timespec *new) {
    return (new->tv_sec - old->tv_sec) * 1000 + (new->tv_nsec - old->tv_nsec) / 1000000;
}

void add_ms(struct timespec *ts, int ms) {
    ts->tv_nsec += ms * 1000000;
    if (ts->tv_nsec > 1000000000) {
        ts->tv_nsec %= 1000000000;
        ts->tv_sec++;
    }
}


void sync_line_state(int i) {
    PHONE *phone = phones + i;
    PHONE *connected_phone;
    if (phone->playing_recording) {
        int recording_number = phone->recording_number;
        int recording_length = recording[recording_number].length;
        uint32_t *data = recording[recording_number].data;
        phone->recording_index %= recording_length;
        struct timespec cur_clock;
        clock_gettime(CLOCK_MONOTONIC_RAW, &cur_clock);
        //sprintf(debug, "cur clock: %ld", cur_clock / CLOCKS_PER_SEC * 1000);
        int packets_to_send = clock_diff_ms(&phone->last_tx, &cur_clock);
        if (packets_to_send > 3) {
            packets_to_send = 3;
            clock_gettime(CLOCK_MONOTONIC_RAW, &phone->last_tx);
        } else {
            add_ms(&phone->last_tx, packets_to_send);
        }
        for (int r = 0; r < packets_to_send; r++) {
            uint32_t raw_data = data[phone->recording_index++];
            uint32_t swapped = ((raw_data>>24)&0xff) | // move byte 3 to byte 0
                    ((raw_data<<8)&0xff0000) | // move byte 1 to byte 2
                    ((raw_data>>8)&0xff00) | // move byte 2 to byte 1
                    ((raw_data<<24)&0xff000000); // byte 0 to byte 3
            queue_push(&phone->txd, swapped);
        }
    }
    if (phone->state == phone_idle &&
        phone->line_state != line_idle &&
        phone->line_state != line_request_ring) {
        if (phone->connected_device != NOT_CONNECTED) {
            int c = phone->connected_device;
            fprintf(logfile, "Clearing connected device %d\n    ", c);
            phones[c].connected_device = NOT_CONNECTED;
            phone->connected_device = NOT_CONNECTED;
        }
        phone->line_state = line_idle;
    }
    switch(phone->line_state) {
        case line_idle:
            if (phone->playing_recording) {
                phone->playing_recording = false;
            }
            if (phone->state == phone_dial) {
                phone->line_state = line_dial;
                clock_gettime(CLOCK_MONOTONIC_RAW, &phone->last_tx);
                phone->playing_recording = true;
                phone->recording_number = DIALTONE_RECORDING;
            }
            if (phone->digit_count > 0) {
                phone->digit_count = 0;
                phone->digits[0] = '\0';
                phone->playing_recording = false;
                phone->recording_index = 0;
            }
            break;
        case line_dial:
            if (phone->digit_count > 0) {
                phone->playing_recording = false;
            }
            int dialed_line;
            dialed_line = match_dialplan(phone->digits);
            if (phone->connected_device == NOT_CONNECTED
                && dialed_line != -1) {
                fprintf(logfile, "Line %d calling line %d\n", i, dialed_line);
                phone->connected_device = dialed_line;
                phone->line_state = line_awaiting_remote_ring;
                phone->playing_recording = true;
                phone->recording_number = RINGTONE_RECORDING;
                phone->recording_index = 0;
                phones[dialed_line].pending_command = RING_COMMAND;
                phones[dialed_line].connected_device = i;
                phones[dialed_line].line_state = line_request_ring;
                clock_gettime(CLOCK_MONOTONIC_RAW, &phones[dialed_line].last_tx);
            }
            break;
        case line_awaiting_remote_ring:
            if (phone->connected_device == NOT_CONNECTED) {
                fprintf(logfile, "line %d no longer connected, to busy\n", i);
                phone->line_state = line_busy_signal;
                clock_gettime(CLOCK_MONOTONIC_RAW, &phone->last_tx);
                phone->playing_recording = true;
                phone->recording_number = BUSY_RECORDING;
                return;
            }
            connected_phone = phones + phone->connected_device;
            if (connected_phone->line_state == line_traffic) {
                fprintf(logfile, "line %d remote terminal in traffic, go to traffic\n", i);
                phone->pending_command = PLAINTEXT_COMMAND;
                phone->line_state = line_traffic;   
                phone->playing_recording = false;
            } else if (connected_phone->line_state == line_unreachable) {
                fprintf(logfile, "line %d remote terminal unreachable, to busy\n", i);
                phone->line_state = line_busy_signal;
                clock_gettime(CLOCK_MONOTONIC_RAW, &phone->last_tx);
                phone->playing_recording = true;
                phone->recording_number = BUSY_RECORDING;
            }
            break;
        case line_request_ring:
            if (phone->state == phone_unreachable) {
                phone->line_state = line_unreachable;
                phone->connected_device = NOT_CONNECTED;
            }
            if (phone->state == phone_ring) {
                fprintf(logfile, "line %d ringing\n", i);
                phone->line_state = line_ringing;
            }
            break;
        case line_ringing:
            if (phone->connected_device == NOT_CONNECTED) {
                phone->pending_command = RING_DISMISS_COMMAND;
                fprintf(logfile, "line %d Not Connected, Dismissing ring\n", i);
            } else {
                if (phone->state == phone_traffic) {
                    fprintf(logfile, "line %d in state traffic, transition to line_traffic\n", i);
                    phone->line_state = line_traffic;
                }
            }
            break;
        case line_traffic:
            if (phone->playing_recording) {
                phone->playing_recording = false;
                phone->recording_index = 0;
            }
            if (phone->state == phone_idle) {
                if (phone->connected_device != NOT_CONNECTED) {
                    connected_phone = phones + phone->connected_device;
                    connected_phone->connected_device = NOT_CONNECTED;
                }
                phone->line_state = line_idle;
                return;
            }
            if (phone->connected_device == NOT_CONNECTED) {
                fprintf(logfile, "Phone %d disconnect command\n", i);
                phone->pending_command = DISCONNECT_COMMAND;
                phone->line_state = line_dial;
                phone->digit_count = 0;
                phone->digits[0] = '\0';
                phone->playing_recording = true;
                phone->recording_number = DIALTONE_RECORDING;
                phone->recording_index = 0;
                clock_gettime(CLOCK_MONOTONIC_RAW, &phone->last_tx);
                return;
            }
            connected_phone = phones + phone->connected_device;
            if (connected_phone->line_state == line_unreachable) {
                fprintf(logfile, "line %d connected phone unreachable 2, to busy\n", i);
                phone->line_state = line_busy_signal;
                clock_gettime(CLOCK_MONOTONIC_RAW, &connected_phone->last_tx);
                phone->playing_recording = true;
                phone->recording_number = BUSY_RECORDING;
            }
            break;
        case line_remote_hangup:
            break;
        case line_busy_signal:
            break;
        case line_unreachable:
            if (phone->connected_device != NOT_CONNECTED) {
                connected_phone = phones + phone->connected_device;
                connected_phone->connected_device = NOT_CONNECTED;
                phone->connected_device = NOT_CONNECTED;
            }
            break;
    }

}

void* usb_worker(void *unused) {
    int transferred_size;
    for (int i = 0; i < MAX_PHONES; i++) {
        phones[i].connected_device = NOT_CONNECTED;
    }
    while (thread_run) {
        for (int s = 0; s < open_devices; s++) {
            struct libusb_device_handle *dh = dnvt_sw[s];
            int config = libusb_bulk_transfer((struct libusb_device_handle *)dh, 0x82, rx_buf, 64, &transferred_size, 1000);
            if (config < 0) {
                printf("ERROR: No data transmitted to device %x, err %s\n\n",DEV_ID, libusb_error_name(config));
                thread_run = false;
                continue;
            }
            memcpy(&host_packet, rx_buf, sizeof(host_packet));
            bool should_send_device_packet = false;
            device_packet.data_lengths = 0;
            for (int p = 0; p < 4; p++) {
                int i = s*4 + p;
                // parse incoming packet
                PHONE *phone = phones + i;
                phone->state = (host_packet.phone_states >> p*4) & 0xf;
                /*
                if (i == 1) {
                    static uint8_t last_phone_state;
                    if (last_phone_state != phone->state) {
                        fprintf(logfile, "Phone %d state from %d to %d\n", i, last_phone_state, phone->state);
                        last_phone_state = phone->state;
                    }
                }*/
                sync_line_state(i);
                uint8_t rx_data_len = (host_packet.data_lengths >> p*2) & 0x3;
                for (int d = 0; d < rx_data_len; d++) {
                    if (phone->connected_device != NOT_CONNECTED) {
                        queue_push(&phones[phone->connected_device].txd, host_packet.data[p][d]);
                    } else {
                        queue_push(&phone->rxd, host_packet.data[p][d]);
                    }
                }
                if (host_packet.phone_digits[p]) {
                    phone->digits[phone->digit_count++] = host_packet.phone_digits[p];
                    phone->digits[phone->digit_count] = '\0';
                }
                // assemble outgoing packet
                uint8_t output_size = queue_size(&phone->txd);
                if (output_size > 0) {
                    should_send_device_packet = true;
                    if (output_size > 3) {
                        output_size = 3;
                    }
                    for (int j = 0; j < output_size; j++) {
                        uint32_t data = queue_pop(&phone->txd);
                        device_packet.data[p][j] = data;
                    }
                    device_packet.data_lengths |= output_size << (p * 2);
                }
                if (phone->pending_command) {
                    should_send_device_packet = true;
                    device_packet.phone_commands[p] = phone->pending_command;
                    phone->pending_command = NO_COMMAND;
                } else {
                    device_packet.phone_commands[p] = 0;
                }
            }
            if (should_send_device_packet) {
                for (int t = 0; t < 4; t++) {
                    if (device_packet.phone_commands[t]) {
                        fprintf(logfile, "Sending command to %d\n", t);
                    }
                }
                device_packets_sent++;
                memcpy(tx_buf, &device_packet, sizeof(DEVICE_PACKET));
                //sprintf(debug, "phone1_packet: %d", )
                int config = libusb_bulk_transfer((struct libusb_device_handle *)dh, 0x01, tx_buf, sizeof(DEVICE_PACKET), &transferred_size, 1000);
                if (config < 0) {
                    fprintf(logfile, "ERROR: No data transmitted to device %x, err %s\n\n",DEV_ID, libusb_error_name(config));
                    thread_run = false;
                    continue;
                }
            }
        }
        usleep(750);
    }
    pthread_exit(NULL);
}

void firmware_update(struct libusb_device_handle *dh) {

    // set fields for the setup packet as needed              
    uint8_t       bmReqType = USB_CONTROL_VENDOR_MESSAGE;   // the request type (direction of transfer)
    uint8_t            bReq = DNVT_REQUEST_STATUS;   // the request field for this packet
    uint16_t           wVal = 0;   // the value field for this packet
    uint16_t         wIndex = 0;   // the index field for this packet
    unsigned char   data[64];   // the data buffer for the in/output data
    uint16_t           wLen = 64;   // length of this setup packet 
    unsigned int     to = 1000;       // timeout duration (if transfer fails)

    // transfer the setup packet to the USB device
    int config =     
    libusb_control_transfer(dh,bmReqType,bReq,wVal,wIndex,data,wLen,to);

    if (config < 0) {
        errx(1,"ERROR: No data transmitted to device %x, err %s\n\n",DEV_ID, libusb_error_name(config));
    }
    return;
}

void usb_test(struct libusb_device_handle *dh) {
    unsigned char buf[64];
    int transferred_size;
    printf("transferred %i bytes\n", transferred_size);
    printf("buffer: ");
    for (int i = 0; i < 64; i++) {
        printf("%02X:", buf[i]);
    }
    printf("\n");
}

void open_recording(char *path, int index) {
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        errx(1, "recording %s missing", path);
    }
    struct stat stat;
    fstat(fileno(fp), &stat);
    recording[index].data = malloc(stat.st_size);
    recording[index].length = stat.st_size / 4;
    size_t read_bytes = fread(recording[index].data, sizeof(uint32_t), stat.st_size/4, fp);
    printf("recording %s read %ld bytes, first word %08x\n", path, read_bytes, recording[index].data[0]);
}


int main() {
    logfile= fopen("logfile.txt", "a");
    setlinebuf(logfile);
    if (!logfile) {
        errx(1, "Logfile open failed\n");
    }

    fprintf(logfile, "DNVT Start\n");
    int init = libusb_init(NULL); // NULL is the default libusb_context
    int config;
    if (init < 0) {
        errx(1,"\n\nERROR: Cannot Initialize libusb\n\n");  
    }
    open_recording("dialtone2.cvs", DIALTONE_RECORDING);
    open_recording("busy.cvs", BUSY_RECORDING);
    open_recording("ringtone.cvs", RINGTONE_RECORDING);
    open_recording("rick.cvs", RICKROLL_RECORDING);
    struct libusb_device **dl = NULL;
    int devices = libusb_get_device_list(NULL, &dl);
    for (int i = 0; i < devices; i++) {
        struct libusb_device *device = dl[i];
        struct libusb_device_descriptor desc= {0};

        int rc = libusb_get_device_descriptor(device, &desc);
        assert(rc == 0);
        printf("Vendor:Device = %04x:%04x v %02x.%02x\n", desc.idVendor, desc.idProduct, desc.bcdDevice >> 8, desc.bcdDevice);     
        if (desc.idVendor == 0xCAFE) {
            unsigned char serial_number[20];
            libusb_open(device, &dnvt_sw[open_devices]);
            int l = libusb_get_string_descriptor_ascii(dnvt_sw[open_devices], desc.iSerialNumber, dev_serials[open_devices], 20);
            printf("Serial: %s\n", dev_serials[open_devices]);
            open_devices++;
        }
    }
    printf("Sizeof clock %ld, clocks per sec %ld\n", sizeof(clock_t), CLOCKS_PER_SEC);
    //struct libusb_device_handle *dh = NULL; // The device handle
    //dh = libusb_open_device_with_vid_pid(NULL,MFGR_ID,DEV_ID);
    //if (!dh) {
      //  errx(1,"\n\nERROR: Cannot connect to device %d\n\n",DEV_ID);
    //}
    //struct libusb_device_descriptor *dd = NULL;
    //config = libusb_get_device_descriptor(dh, dd);
    //printf("Version: %02x.%02x\n", dd->bcdDevice >> 8, dd->bcdDevice);
    /*
    int configuration;
    config = libusb_get_configuration(dh, &configuration);
    if (config < 0) {
        errx(1,"ERROR: cannot get config %x, err %s\n\n",DEV_ID, libusb_error_name(config));
    }
    printf("configuration %d\n", configuration);*/
    for (int i = 0; i < open_devices; i++) {
        config = libusb_claim_interface(dnvt_sw[i], 0);
        if (config < 0) {
            errx(1,"ERROR: cannot claim interface %x, err %s\n\n",DEV_ID, libusb_error_name(config));
        }
        //printf("claimed interface 0\n");
        //printf("sizeof device packet %ld\n", sizeof(DEVICE_PACKET));
    }
    
    //printf("phone states: %x\n", host_packet.phone_states);
    // now you can use libusb_bulk_transfer to send raw data to the device
    bool quit = false;
    initscr();
    raw();
    keypad(stdscr, true);
    noecho();
    timeout(100);
    pthread_t th1;
    pthread_create(&th1, NULL, usb_worker, (void*) NULL);
    while (!quit && thread_run) {			/* Start curses mode 		  */
        int row, col;
        getmaxyx(stdscr,row,col);
        clear();
        mvaddstr(0,0,"Official DNVT Console v0.1");
        mvprintw(1,0,"this is hopefully line 2");
        mvprintw(2,0,"You have r: %d, c: %d", row, col);
        for (int i = 0; i < open_devices * 4; i++) {
            PHONE *phone = phones + i;
            mvprintw(i+3, 0, "Phone %d: %x\n",i+1,phone->state);
            mvprintw(i+3, 13, "L: %d", phone->line_state);
            mvprintw(i+3, 20, "N: %s", phone->digits);
            mvprintw(i+3, 40, "R: %d", phone->recording_index);
        }
        //mvprintw(7,0, "Raw states: %04x", host_packet.phone_states);
        //mvprintw(8,0, "Device Packets: %d", device_packets_sent);
        //mvprintw(9,0, "Last phone1 cmd: %d", device_packet.phone_commands[1]);
        mvprintw(30,0, "%s", debug);
        /*for (int b = 0; b < 64; b++) {
            int row_ind = b / 15;
            int col_ind = (b % 15) *3;
            mvprintw(row_ind + 10, col_ind, "%02x", tx_buf[b]);
        }*/
        refresh();			/* Print it on to the real screen */
        char c = getch();			/* Wait for user input */
        if (c == 'q') {
            quit = true;
        } else if (c == 'r') {
            phones[1].pending_command = RING_COMMAND;
        } else if (c == 'd') {
            phones[1].pending_command = RING_DISMISS_COMMAND;
        }
        
    }
    endwin();			/* End curses mode		  */
    thread_run = false;
    pthread_join(th1, (void *) NULL);
    for (int i = 0; i< open_devices; i++) {
        libusb_release_interface(dnvt_sw[i],0);
    }
    libusb_exit(NULL);
}
