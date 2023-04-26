CFLAGS += -Wall -ggdb3 -pthread
CFLAGS += $(shell pkg-config --cflags-only-I libusb-1.0)
LDLIBS += $(shell pkg-config --libs libusb-1.0)
LDLIBS += $(shell pkg-config --libs ncurses)

main:

.PHONY: clean

clean:
	$(RM) main
