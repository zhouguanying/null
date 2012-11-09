CC  = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++

CFLAGS  += -Wall -c -mcpu=arm926ej-s -O2 -I. -I./include -I../udt4/app -DIPED_98 -I./akmedialib/include/
LDFLAGS += -L./alsa/lib/ -L./lib -lpthread -lasound -ludt -lstdc++ -lm -lspeexdsp \
		   -L./akmedialib/lib/ -lakuio  -lakvideocodec -lakaudiocodec -lasound -lakmedialib -lak2dsys

SRC  = $(wildcard *.c)
SRC += fatty.c 
OBJ  = $(patsubst %.c, build/%.o, $(SRC))
OBJ += build/stun.o build/stun-camera.o 

OBJ += libstdc++.a lib/libspeexdsp.a lib/AMR_NB_ENC.a lib/libudt.a \
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
	cp $(TARGET) /home/zgy/test/ -f

all: $(TARGET)

clean:
	rm -f build/*.o $(TARGET)

.PHONY: all clean
