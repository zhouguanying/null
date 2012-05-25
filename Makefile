
CC=$(CROSS_COMPILE)gcc
LD=$(CROSS_COMPILE)g++
CFLAGS +=-Wall -c -mcpu=arm926ej-s -O2  -I/root/ipcam/alsa/include/
LDFLAGS += -lpthread  -lamr -L./  -L/root/ipcam/alsa/lib/ -lasound   -ludt -L./ -lstdc\+\+ -L./ -lm -L./

SRCS = $(wildcard *.c)
OBJS = $(patsubst %c, %o, $(SRCS))
OBJS += nand_file.oo  udt.oo    libudt.a libstdc\+\+.a libm.a amrcoder.oo amrdecoder.oo libamr.a
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
