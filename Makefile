all: sh

sh: sh.o utils.o
	gcc -o $@ $^

utils.o: error_handlers.c
	gcc -g -c -o $@ $^

sh.o: sh.c
	gcc -g -c -o $@ $^

clean:
	rm -f *.o sh
