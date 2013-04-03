all: sh

sh: sh.o utils.o
	gcc -o $@ $^

utils.o: error_handlers.c
	sh build_ename.sh > ename.c.inc
	gcc -g -c -o $@ $^

sh.o: sh.c
	gcc -g -c -o $@ $^

clean:
	rm -f *.o ename.c.inc sh
