CC  = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++

CFLAGS  += -Wall -c -mcpu=arm926ej-s -O2 -I. -I./include -I../udt4/app -DIPED_98 -I./akmedialib/include/ -I./akmedialib/include/akmedialib
LDFLAGS += -L./alsa/lib/ -L./lib -lpthread -lasound -ludt -lstdc++ -lm -lspeexdsp \
		   -L./akmedialib/lib/ -lakuio  -lakvideocodec -lakaudiocodec -lasound -lakmedialib -lak2dsys \
		   -lakuio -lakimage

SRC  = $(wildcard *.c)
SRC += fatty.c 
OBJ  = $(patsubst %.c, build/%.o, $(SRC))
OBJ += build/stun.o build/stun-camera.o 

OBJ += libstdc++.a lib/libspeexdsp.a lib/AMR_NB_ENC.a lib/libudt.a \
       lib/udttools.oo lib/libencoder.a lib/libdecoder.a \
       lib/libspc.a lib/libfipop.a

TEST_OBJ = build/test.o
ENCODER_OBJ = build/VideoEncode.o


OBJ_TEST =  $(filter-out $(ENCODER_OBJ),$(OBJ)) 
OBJ_ENCODER =  $(filter-out $(TEST_OBJ),$(OBJ))
 
%.o: ../%.c
	$(CC) $(CFLAGS) -o $@ $<

%.o: ../../fat/%.c
	$(CC) $(CFLAGS) -o $@ $<

%.o: ../../udt4/app/%.cpp
	$(CXX) $(CFLAGS) -o $@ $<

TARGET = test

$(TARGET): $(OBJ_TEST)
	$(CXX) -o $@ $^ $(LDFLAGS)
	arm-none-linux-gnueabi-strip $(TARGET) 

encoder: $(OBJ_ENCODER)
	$(CXX) -o $@ $^ $(LDFLAGS)
	arm-none-linux-gnueabi-strip encoder

all: $(TARGET)

clean:
	rm -f build/*.o $(TARGET)

.PHONY: all clean
