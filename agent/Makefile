CC=gcc
CFLAGS=-g -m64 -Wall -O1
LDFLAGS=-g -lm -ljansson -lpthread

at_agent: fsops.o cpuops.o iodevops.o ifaceops.o at_agent.o
	${CC} ${LDFLAGS} ../ini.o fsops.o cpuops.o iodevops.o ifaceops.o at_agent.o -o at_agent
	
fsops.o: fsops.c ../at.h at_agent.h
	${CC} ${CFLAGS} -o fsops.o -c fsops.c
	
cpuops.o: cpuops.c ../at.h at_agent.h
	${CC} ${CFLAGS} -o cpuops.o -c cpuops.c
 
iodevops.o: iodevops.c ../at.h at_agent.h
	${CC} ${CFLAGS} -o iodevops.o -c iodevops.c

ifaceops.o: ifaceops.c ../at.h at_agent.h
	${CC} ${CFLAGS} -o ifaceops.o -c ifaceops.c
	
clean:
	rm -f *.o at_agent core.*