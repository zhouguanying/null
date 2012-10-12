CC  = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++

CFLAGS  += -Wall -c -mcpu=arm926ej-s -O2 -I. -I./include -I../udt4/app
LDFLAGS += -L./alsa/lib/ -L./lib -lpthread -lasound -ludt -lstdc++ -lm -lspeexdsp

SRC  = $(wildcard *.c)
SRC += fatty.c
OBJ  = $(patsubst %.c, build/%.o, $(SRC))
OBJ += build/stun.o build/stun-camera.o

OBJ += lib/libspeexdsp.a lib/AMR_NB_ENC.a lib/libudt.a \
       lib/udttools.oo lib/libencoder.a lib/libdecoder.a \
       lib/libspc.a lib/libfipop.a

%.o: ../%.c
	$(CC) $(CFLAGS) -o $@ $<

%.o: ../../fat/%.c
	$(CC) $(CFLAGS) -o $@ $<

%.o: ../../udt4/app/%.cpp
	$(CXX) $(CFLAGS) -o $@ $<

TARGET = test

$(TARGET): $(OBJ)
	$(CXX) -o $@ $^ $(LDFLAGS)
	arm-none-linux-gnueabi-strip $(TARGET) 

all: $(TARGET)

clean:
	rm -f build/*.o $(TARGET)

.PHONY: all clean
