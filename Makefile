GCC = $(CROSS_COMPILE)gcc
SYS := $(shell $(GCC) -dumpmachine)
ifneq (, $(findstring x86_64, $(SYS)))
	OSFLAGS = -Ofast -fPIC -march=native -mtune=native -mfpmath=sse -Wconversion -Wunreachable-code -Wstrict-prototypes 
endif
ifneq (, $(findstring arm, $(SYS)))
	ifneq (, $(findstring gnueabihf, $(SYS)))
		OSFLAGS = -Ofast -mfloat-abi=hard -mfpu=vfp -march=armv6 -Wconversion -Wunreachable-code -Wstrict-prototypes 
	endif
	ifneq (, $(findstring gnueabi, $(SYS)))
		OSFLAGS = -Ofast -mfloat-abi=hard -mfpu=vfp -march=armv6 -Wconversion -Wunreachable-code -Wstrict-prototypes 
	endif	
	ifneq (, $(findstring gnueabisf, $(SYS)))
		OSFLAGS = -Ofast -mfloat-abi=soft -mfpu=vfp -march=armv6 -Wconversion -Wunreachable-code -Wstrict-prototypes 
	endif
endif
ifneq (, $(findstring amd64, $(SYS)))
	OSFLAGS = -O3 -fPIC -march=native -mtune=native -mfpmath=sse -Wno-conversion
endif
CFLAGS = -ffast-math $(OSFLAGS) -Wfloat-equal -Wshadow -Wpointer-arith -Wcast-align -Wstrict-overflow=5 -Wwrite-strings -Waggregate-return -Wcast-qual -Wswitch-default -Wswitch-enum -Wformat=2 -g -Wall -I. -I.. -Ilibs/pilight/ -Ilibs/protocols/ -Ilibs/hardwares/ -Ilibs/lirc/ -I/usr/include/ -L/usr/lib/arm-linux-gnueabihf/ -pthread -lm
SUBDIRS = libs/pilight libs/protocols libs/hardwares libs/websockets
SRC = $(wildcard *.c)
INCLUDES = $(wildcard protocols/*.h) $(wildcard protocols/*.c) $(wildcard hardwares/*.h) $(wildcard hardwares/*.c) $(wildcard libs/pilight/*.h) $(wildcard libs/websockets/*.c) $(wildcard libs/websockets/*.h) $(wildcard libs/websockets/*.c)
PROGAMS = $(patsubst %.c,pilight-%,$(SRC))
LIBS = libs/pilight/pilight.o libs/protocols/protocols.o libs/hardwares/hardwares.o libs/websockets/websockets.o

.PHONY: subdirs $(SUBDIRS)

subdirs: $(SUBDIRS) all

$(SUBDIRS):
	$(MAKE) -C $@

all: $(LIBS) libpilight.so.1 libpilight.a $(PROGAMS) 

libpilight.so.1: $(LIBS)
	$(GCC) $(LIBS) -shared -o libpilight.so.1 -lpthread -lm
	
libpilight.a: $(LIBS)
	$(CROSS_COMPILE)ar -rsc libpilight.a $(LIBS)

pilight-daemon: daemon.c $(INCLUDES) $(LIBS) libpilight.so.1
	$(GCC) $(CFLAGS) -o $@ $(patsubst pilight-%,%.c,$@) libpilight.so.1

pilight-send: send.c $(INCLUDES) $(LIBS) libpilight.so.1
	$(GCC) $(CFLAGS) -o $@ $(patsubst pilight-%,%.c,$@) libpilight.so.1

pilight-receive: receive.c $(INCLUDES) $(LIBS) libpilight.so.1
	$(GCC) $(CFLAGS) -o $@ $(patsubst pilight-%,%.c,$@) libpilight.so.1

pilight-debug: debug.c $(INCLUDES) $(LIBS) libpilight.so.1
	$(GCC) $(CFLAGS) -o $@ $(patsubst pilight-%,%.c,$@) libpilight.so.1

pilight-learn: learn.c $(INCLUDES) $(LIBS) libpilight.so.1
	$(GCC) $(CFLAGS) -o $@ $(patsubst pilight-%,%.c,$@) libpilight.so.1

pilight-control: control.c $(INCLUDES) $(LIBS) libpilight.so.1
	$(GCC) $(CFLAGS) -o $@ $(patsubst pilight-%,%.c,$@) libpilight.so.1

install:
	[ -d /usr/share/images/pilight/ ] && rm -r /usr/share/images/pilight/ || true
	
	install -m 0755 -d /usr/local/lib
	install -m 0755 -d /usr/local/sbin
	install -m 0755 -d /etc/pilight
	install -m 0755 -d /usr/local/share/pilight/
	install -m 0655 pilight-daemon /usr/local/sbin/
	install -m 0655 pilight-send /usr/local/sbin/
	install -m 0655 pilight-receive /usr/local/sbin/
	install -m 0655 pilight-control /usr/local/sbin/
	install -m 0655 pilight-debug /usr/local/sbin/
	install -m 0655 pilight-learn /usr/local/sbin/
	install -m 0655 libpilight.so.1 /usr/local/lib/
	install -m 0644 settings.json-default /etc/pilight/
	install -m 0644 web/index.html /usr/local/share/pilight/
	install -m 0644 web/pilight.css /usr/local/share/pilight/
	install -m 0644 web/pilight.js /usr/local/share/pilight/
	install -m 0644 web/favicon.ico /usr/local/share/pilight/
	install -m 0644 web/logo.png /usr/local/share/pilight/
	install -m 0644 web/battery_green.png /usr/local/share/pilight/
	install -m 0644 web/battery_red.png /usr/local/share/pilight/
	install -m 0644 web/apple-touch-icon-iphone-retina-display.png /usr/local/share/pilight/
	install -m 0644 web/apple-touch-icon-ipad.png /usr/local/share/pilight/
	install -m 0644 web/apple-touch-icon-iphone.png /usr/local/share/pilight/	
	[ ! -f /usr/local/bin/gpio ] && install -m 0755 deps/gpio /usr/local/bin/ || true
	[ ! -f /etc/pilight/settings.json ] && mv /etc/pilight/settings.json-default /etc/pilight/settings.json || true
	ln -sf /usr/local/lib/libpilight.so.1 /usr/local/lib/libpilight.so
	ldconfig
	
	
clean:
	rm pilight-* >/dev/null 2>&1 || true
	rm *pilight*.so* || true
	rm *pilight*.a* || true
	$(MAKE) -C libs/ $@; \
	$(MAKE) -C protocols/ $@; \
	
dist-clean:
	rm pilight-* >/dev/null 2>&1 || true
	rm *pilight*.so* || true
	rm *pilight*.a* || true
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir $@; \
	done
