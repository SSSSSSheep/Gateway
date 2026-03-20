CC:=$(CROSS_COMPILE)gcc

BOARD_DIR := $(shell pwd)/
PEER := root@192.168.1.6

CFLAGS += -Wall -Wextra

CFLAGS += -I.
CFLAGS += -Ithirdparty
CFLAGS += -Iapp
CFLAGS += -Idaemon
CFLAGS += -Iota

ifdef SYSROOT
	CFLAGS += --sysroot=$(SYSROOT)
endif

LDLIBS += -lpaho-mqtt3c
LDLIBS += -lcurl
LDLIBS += -lcrypto

SRC += $(shell find app -name "*.c"	-type f)
SRC += $(shell find daemon -name "*.c"	-type f)
SRC += $(shell find ota -name "*.c"	-type f)
SRC += $(shell find thirdparty -name "*.c"	-type f)

OBJ := $(SRC:.c=.o)

TARGET := gateway

.PHONY: all, clean

all: $(TARGET)

clean:
	@-rm -f $(TARGET) $(OBJ) main.o

$(TARGET): main.o $(OBJ)
	@-$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

cross-compile:
	@CROSS_COMPILE=$(BOARD_DIR)/toolchain/bin/arm-linux-gnueabihf- \
	 SYSROOT=$(BOARD_DIR)/sysroot \
	 make -j16
	@scp -O $(TARGET) $(PEER):/usr/bin/$(TARGET)
	
cross-init:
	@scp -O init/S99gateway $(PEER):/etc/init.d/S99gateway

%.o: %.c
	@-$(CC) $(CFLAGS) -c $^ -o $@

%_test: test/%_test.o $(OBJ)
	@-$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)
	@-./$@
	@-rm $@ $^

# gcc -Ixxx   为gcc指定include目录