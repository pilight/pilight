CC = $(CROSS_COMPILE)gcc
CFLAGS = -Ofast -ffast-math -mfloat-abi=hard -mfpu=vfp -march=armv6 -Wfloat-equal -Wshadow -Wpointer-arith -Wcast-align -Wstrict-prototypes -Wstrict-overflow=5 -Wwrite-strings -Waggregate-return -Wcast-qual -Wswitch-default -Wswitch-enum -Wconversion -Wunreachable-code -Wformat=2 -g -Wall -static -I. -I.. -I/usr/include/
SRC = $(wildcard *.c)
OBJS = $(SRC:.c=.o)
HEADERS := $(SRC:.c=.h)

all: json.o $(OBJS) libs.o

json.o: json.c json.h
	$(CC) -Ofast -ffast-math -mfloat-abi=hard -mfpu=vfp -march=armv6 -g -static -I. -I.. -I/usr/include/ -c $(patsubst %.o,%.c,$@) $(LIBS)

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $(patsubst %.o,%.c,$@) $(LIBS)

libs.o: $(OBJS)
	$(CROSS_COMPILE)ld -L/usr/local/lib -r $(OBJS) -o libs.o
	
clean:
	rm *.o >/dev/null 2>&1 || true