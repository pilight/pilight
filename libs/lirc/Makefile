GCC = $(CROSS_COMPILE)gcc
SYS := $(shell $(GCC) -dumpmachine)
ifneq (, $(findstring x86_64, $(SYS)))
	OSFLAGS = -fPIC -O0 -march=native -mtune=native -mfpmath=sse
endif
ifneq (, $(findstring arm, $(SYS)))
	ifneq (, $(findstring gnueabihf, $(SYS)))
		OSFLAGS = -O0 -mfloat-abi=hard -mfpu=vfp -march=armv6
	endif
	ifneq (, $(findstring gnueabi, $(SYS)))
		OSFLAGS = -O0 -mfloat-abi=hard -mfpu=vfp -march=armv6
	endif	
	ifneq (, $(findstring gnueabisf, $(SYS)))
		OSFLAGS = -O0 -mfloat-abi=soft -mfpu=vfp -march=armv6
	endif
endif
ifneq (, $(findstring amd64, $(SYS)))
	OSFLAGS = -fPIC -O0 -march=native -mtune=native -mfpmath=sse
endif
CFLAGS = -ffast-math $(OSFLAGS) -g -Wall -static -I. -I..
SRC = $(wildcard *.c)
OBJS = $(SRC:.c=.o)
HEADERS := $(SRC:.c=.h)

all: $(OBJS) lirc.o

%.o: %.c %.h
	$(GCC) $(CFLAGS) -c $(patsubst %.o,%.c,$@)

lirc.o: $(OBJS)
	$(CROSS_COMPILE)ld -L/usr/local/lib -r $(OBJS) -o lirc.o

dist-clean: clean	

clean:
	rm *.o >/dev/null 2>&1 || true