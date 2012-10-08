CC  = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++

CFLAGS  += -Wall -c -mcpu=arm926ej-s -O2 -I./alsa/include -I./include -I../udt4/app
LDFLAGS += -L./alsa/lib/ -L./lib -lpthread -lasound -ludt -lstdc++ -lm -lspeexdsp

SRC  = $(wildcard *.c)
SRC += fatty.c
OBJ  = $(patsubst %.c, build/%.o, $(SRC))

OBJ += lib/libm.a lib/libstdc++.a lib/libspeexdsp.a lib/AMR_NB_ENC.a lib/libudt.a \
       lib/stun.oo lib/stun-camera.oo lib/udttools.oo lib/libencoder.a lib/libdecoder.a \
       lib/libspc.a lib/libfipop.a

%.o: ../%.c
	$(CC) $(CFLAGS) -o $@ $<

%.o: ../../fat/%.c
	$(CC) $(CFLAGS) -o $@ $<

TARGET = test

$(TARGET): $(OBJ)
	$(CXX) -o $@ $^ $(LDFLAGS)
	arm-none-linux-gnueabi-strip $(TARGET) 

all: $(TARGET)

clean:
	rm -f build/*.o $(TARGET)

.PHONY: all clean
