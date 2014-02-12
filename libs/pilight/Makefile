GCC = $(CROSS_COMPILE)gcc
SYS := $(shell $(GCC) -dumpmachine)
OS := $(shell uname)

ifdef CROSS_COMPILE
	INCLUDE = -isystem/usr/include/ -isystem/usr/include/x86_64-linux-gnu/
endif
ifneq (, $(findstring x86_64, $(SYS)))
	OSFLAGS = -Ofast -fPIC -march=native -mtune=native -mfpmath=sse -Wconversion -Wunreachable-code -Wstrict-prototypes 
endif
ifneq (, $(findstring arm, $(SYS)))
	ifneq (, $(findstring gnueabihf, $(SYS)))
		OSFLAGS = -Ofast -mfloat-abi=hard -mfpu=vfp -march=armv6 -Wconversion -Wunreachable-code -Wstrict-prototypes 
	endif
	ifneq (, $(findstring gnueabi, $(SYS)))
		ifeq (, $(findstring FreeBSD, $(SYS)))
			OSFLAGS = -isystem../../../../x-tools/arm-unknown-linux-gnueabi/sysroot/usr/include/ -nostdinc -Ofast -mfloat-abi=hard -mfpu=vfp -march=armv6 -Wconversion -Wunreachable-code -Wstrict-prototypes 
		else
			OSFLAGS = -Ofast -mfloat-abi=hard -mfpu=vfp -march=armv6 -Wconversion -Wunreachable-code -Wstrict-prototypes 
		endif
	endif	
	ifneq (, $(findstring gnueabisf, $(SYS)))
		OSFLAGS = -Ofast -mfloat-abi=soft -mfpu=vfp -march=armv6 -Wconversion -Wunreachable-code -Wstrict-prototypes 
	endif
endif
ifneq (, $(findstring amd64, $(SYS)))
	OSFLAGS = -O3 -fPIC -march=native -mtune=native -mfpmath=sse -Wno-conversion
endif
CFLAGS = -ffast-math $(OSFLAGS) -Wfloat-equal -Wshadow -Wpointer-arith -Wcast-align -Wformat-nonliteral -Wstrict-overflow=5 -Wwrite-strings -Waggregate-return -Wcast-qual -Wswitch-default -Wswitch-enum -Wformat=2 -g -Wall -isystem. -isystem..
SRC = $(wildcard *.c)
OBJS = $(filter-out wiringPi.o wiringPiI2C.o http_lib.o json.o update.o, $(SRC:.c=.o))

all: json.o wiringPi.o wiringPiI2C.o http_lib.o mongoose.o update.o $(OBJS) pilight.o

json.o: json.c json.h
	$(GCC) -ffast-math $(OSFLAGS) -Wno-conversion -g -isystem. -isystem.. -c $(patsubst %.o,%.c,$@)

http_lib.o: http_lib.c http_lib.h
	$(GCC) -ffast-math $(OSFLAGS) -Wno-conversion -g -isystem. -isystem.. -c $(patsubst %.o,%.c,$@)
	
wiringPi.o: wiringPi.c wiringPi.h
	$(GCC) -ffast-math $(OSFLAGS) -w -g -isystem. -isystem.. -pthread -lm -c $(patsubst %.o,%.c,$@)

wiringPiI2C.o: wiringPiI2C.c wiringPiI2C.h
	$(GCC) -ffast-math $(OSFLAGS) -w -g -isystem. -isystem.. -pthread -lm -c $(patsubst %.o,%.c,$@)
	
mongoose.o: mongoose.c mongoose.h
	$(GCC) -ffast-math $(OSFLAGS) -w -g -isystem. -isystem.. -pthread -lm -c $(patsubst %.o,%.c,$@)
		
update.o: update.c update.h
	$(GCC) $(CFLAGS) -lz -c $(patsubst %.o,%.c,$@) -isystem. -isystem.. $(INCLUDE)

%.o: %.c %.h 
	$(GCC) $(CFLAGS) -c $(patsubst %.o,%.c,$@)

pilight.o: $(OBJS) json.o http_lib.o wiringPi.o wiringPiI2C.o update.o
	$(CROSS_COMPILE)ld -L/usr/local/lib -r $(OBJS) json.o http_lib.o wiringPi.o wiringPiI2C.o update.o -o pilight.o

dist-clean: clean

clean:
	rm *.o >/dev/null 2>&1 || true