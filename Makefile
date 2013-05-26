all:
	gcc -O2 -g -Wall -I. -I.. -o receive *.c protocols/* daemons/libhw_module.a
clean:
	rm -rf receive
