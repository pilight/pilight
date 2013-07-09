CC = $(CROSS_COMPILE)gcc
CFLAGS = -Ofast -ffast-math -mfloat-abi=hard -mfpu=vfp -march=armv6 -g -Wall -static -I. -I..
SRC = $(wildcard *.c)
OBJS = $(SRC:.c=.o)
HEADERS := $(SRC:.c=.h)

all: $(OBJS) lirc.o

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $(patsubst %.o,%.c,$@)

lirc.o: $(OBJS)
	$(CROSS_COMPILE)ld -L/usr/local/lib -r $(OBJS) -o lirc.o

clean:
	rm *.o >/dev/null 2>&1 || true