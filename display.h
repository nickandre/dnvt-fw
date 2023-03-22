#include "ssd1306.h"

// I2C defines
// This example will use I2C0 on GPIO8 (SDA) and GPIO9 (SCL) running at 400KHz.
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define I2C_PORT i2c1
#define I2C_SDA 6
#define I2C_SCL 7
#define LINE_SPACE 9
#define CHAR_WIDTH 6
#define NUM_PHONES 4


void update_display();
void init_display();
void display_fw_update();