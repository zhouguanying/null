
CC=$(CROSS_COMPILE)gcc
LD=$(CROSS_COMPILE)g++
CFLAGS +=-Wall -c -mcpu=arm926ej-s -O2  -I/root/ipcam/alsa/include/
LDFLAGS += -lpthread    -L/root/ipcam/alsa/lib/ -lasound   -ludt -L./ -lstdc\+\+ -L./ -lm -L./    -lspeexdsp -L./

SRCS = $(wildcard *.c)
OBJS = $(patsubst %c, %o, $(SRCS))
OBJS += nand_file.oo  udt.oo libm.a   libstdc\+\+.a   libspeexdsp.a AMR_NB_ENC.a libudt.a stun.oo stun-camera.oo
TARGET = test

.PHONY: all clean

all: $(TARGET)
$(TARGET): $(OBJS)
	$(LD) -o $@ $^ $(LDFLAGS)
	arm-none-linux-gnueabi-strip $(TARGET) 
%o: %c
	$(CC) $(CFLAGS) -o $@ $<
clean:
	rm -f *.o $(TARGET)
