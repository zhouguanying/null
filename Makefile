
CC=$(CROSS_COMPILE)gcc 
LD=$(CROSS_COMPILE)g++
CFLAGS +=-Wall -c -mcpu=arm926ej-s -O2  -I/root/ipcam/alsa/include/
LDFLAGS += -lpthread -lencoder  -L./ -lfipop -L./ -lspc -L./ -ldecoder -L./ -lasound -L/root/ipcam/alsa/lib/  -ljrtp -L./ -lstdc\+\+ -L./

SRCS = $(wildcard *.c)
OBJS = $(patsubst %c, %o, $(SRCS))
OBJS += nand_file.oo libencoder.a libdecoder.a libfipop.a libspc.a libjrtp.a rtp.oo libstdc\+\+.a
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
