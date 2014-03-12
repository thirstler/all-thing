CC=gcc
CFLAGS="-Wall"

debug:clean
	$(CC) $(CFLAGS) -g -o all-thing main.c
stable:clean
	$(CC) $(CFLAGS) -o all-thing main.c
clean:
	rm -vfr *~ all-thing
