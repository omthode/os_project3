gompeimalloc: gompeimalloc.c test_gompeimalloc.c
	gcc -o gompeimalloc test_gompeimalloc.c gompeimalloc.c

clean:
	rm -f gompeimalloc