CC = $(CROSS_COMPILE)gcc
CFLAGS = -O3 -g -Wall -static -I. -I.. -I/usr/include/
SRC = $(wildcard *.c)
OBJS = $(SRC:.c=.o)
HEADERS := $(SRC:.c=.h)

all: $(OBJS) libs.o

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $(patsubst %.o,%.c,$@) $(LIBS)

libs.o: $(OBJS)
	$(CROSS_COMPILE)ld -L/usr/local/lib -r $(OBJS) -o libs.o
	
clean:
	rm *.o >/dev/null 2>&1 || true