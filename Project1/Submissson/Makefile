CC=gcc
CFLAGS=-Wall -pthread

all: threadtopk proctopk

threadtopk: threadtopk.c
	$(CC) $(CFLAGS) -o threadtopk threadtopk.c

proctopk: proctopk.c
	$(CC) $(CFLAGS) -o proctopk proctopk.c

clean:
	rm -f threadtopk proctopk

runt: 
	./threadtopk 11 outputt.txt 3 test.txt test2.txt test3.txt
runc:
	./proctopk 11 outputc.txt 3 test.txt test2.txt test3.txt
timet:
	time ./threadtopk 1000 outputt.txt 5 test.txt test2.txt test3.txt test4.txt test5.txt test6.txt test7.txt test8.txt test9.txt test10.txt 
timec:
	time ./proctopk 1000 outputc.txt 5 test.txt test2.txt test3.txt test4.txt test5.txt test6.txt test7.txt test8.txt test9.txt test10.txt
