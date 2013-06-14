CC = gcc
CFLAGS = -O3 -g -Wall -I. -I.. -Ilibs/ -Iprotocols/ -Ilirc/ -lconfig -lpthread -lm
SUBDIRS = libs protocols lirc
SRC = $(wildcard *.c)
INCLUDES = $(wildcard protocols/*.o) $(wildcard lirc/*.o) $(wildcard libs/*.h) $(wildcard libs/*.o)
PROGAMS = $(patsubst %.c,433-%,$(SRC))
LIBS = libs/libs.o protocols/protocols.o lirc/lirc.o

.PHONY: subdirs $(SUBDIRS)

subdirs: $(SUBDIRS) all

$(SUBDIRS):
	$(MAKE) -C $@
 
all: $(LIBS) $(OBJS) $(PROGAMS) 
 
$(PROGAMS): $(INCLUDES) $(LIBS)
	$(CC) $(CFLAGS) -o $@ $(patsubst 433-%,%.c,$@) $(LIBS)
	
clean:
	rm 433-* >/dev/null 2>&1 || true
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir $@; \
	done