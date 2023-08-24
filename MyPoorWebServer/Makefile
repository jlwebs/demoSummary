all: myhttp.o threadPool.o
threadPool.o: threadPool.cpp
myhttp.o: httpd.c threadPool.h
	gcc -W -Wall -o myhttp httpd.c -lpthread

clean:
	rm myhttp.o threadPool.o
