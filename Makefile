CC=$(CROSS_COMPILE)gcc
LD=$(CROSS_COMPILE)g++
CFLAGS +=-Wall -c -mcpu=arm926ej-s -O2 -I./alsa/include/
LDFLAGS += -lpthread -L./alsa/lib/ -lasound -ludt -L./ -lstdc\+\+ -lm -lspeexdsp

SRCS = $(wildcard *.c)
OBJS = $(patsubst %c, %o, $(SRCS))
OBJS +=libm.a libstdc\+\+.a libspeexdsp.a AMR_NB_ENC.a libudt.a stun.oo stun-camera.oo udttools.oo libencoder.a libdecoder.a libspc.a libfipop.a
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
