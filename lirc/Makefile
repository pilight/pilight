CC = gcc
CFLAGS = -O3 -g -Wall -static -I. -I..
SRC = $(wildcard *.c)
OBJS = $(SRC:.c=.o)
HEADERS := $(SRC:.c=.h)

all: $(OBJS) lirc.o

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $(patsubst %.o,%.c,$@)

lirc.o: $(OBJS)
	ld -L/usr/local/lib -r $(OBJS) -o lirc.o

clean:
	rm *.o >/dev/null 2>&1 || true