all:
	gcc -O2 -g -Wall -I. -I.. -I daemons/. -I drivers/. -I protocols/. -o receive *.c protocols/* daemons/libhw_module.a
clean:
	rm -rf receive
