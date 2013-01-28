all:
	gcc -Wall -pedantic -o receive receive.cpp -L/usr/local/lib -lwiringPi -lstdc++
	gcc -Wall -pedantic -o send send.cpp -L/usr/local/lib -lwiringPi -lstdc++
	gcc -Wall -pedantic -o receiveElro receiveElro.cpp -L/usr/local/lib -lwiringPi -lstdc++
	gcc -Wall -pedantic -o sendElro sendElro.cpp -L/usr/local/lib -lwiringPi -lstdc++
	gcc -Wall -pedantic -o genLirc genLirc.cpp -L/usr/local/lib -lwiringPi -lstdc++
	gcc -Wall -pedantic -o learn learn.cpp -L/usr/local/lib -lwiringPi -lstdc++ 
clean:
	rm -rf send receive receiveElro sendElro genLirc
