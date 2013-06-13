CC = gcc
CFLAGS = -Wall -I. -I..
SRC = $(wildcard *.c)
OBJS = $(SRC:.c=.o)
HEADERS := $(SRC:.c=.h)
INCLUDES = $(wildcard ../*.h)

all: $(OBJS) libs.o

%.o: %.c %.h $(INCLUDES)
	$(CC) $(CFLAGS) -c $(patsubst %.o,%.c,$@)

libs.o:
	ld -L/usr/local/lib -r $(OBJS) -o libs.o
	
clean:
	rm *.o >/dev/null 2>&1 || true