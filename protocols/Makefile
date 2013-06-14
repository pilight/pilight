CC = gcc
CFLAGS = -O2 -g -Wall -static -I. -I.. -I../libs/
SRC = $(wildcard *.c)
OBJS = $(SRC:.c=.o)
HEADERS := $(SRC:.c=.h)
INCLUDES = $(wildcard ../libs/*.h)

all: $(OBJS) protocols.o

%.o: %.c %.h $(INCLUDES)
	$(CC) $(CFLAGS) -c $(patsubst %.o,%.c,$@)

protocols.o:
	ld -L/usr/local/lib -r $(OBJS) -o protocols.o
	
clean:
	rm *.o >/dev/null 2>&1 || true