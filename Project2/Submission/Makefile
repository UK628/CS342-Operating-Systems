CC = gcc
CFLAGS = -Wall -Werror
LDFLAGS = -lm

all: mps mps_cv

mps: mps.o
	$(CC) $(CFLAGS) -o mps mps.o $(LDFLAGS)

mps_cv: mps_cv.o
	$(CC) $(CFLAGS) -o mps_cv mps_cv.o $(LDFLAGS)

mps.o: mps.c
	$(CC) $(CFLAGS) -c mps.c

mps_cv.o: mps_cv.c
	$(CC) $(CFLAGS) -c mps_cv.c

clean:
	rm -f mps mps_cv *.o
