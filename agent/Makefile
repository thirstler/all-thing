CJSON_OBJS:=../cJSON.o
CC=gcc
CFLAGS=-g -m64 -Wall -O1
LDFLAGS=-g -lm -lpthread -luuid
AGENT_OBJECTS=../ini.o cpuops.o at_agent.o fsops.o iodevops.o ifaceops.o ../cJSON.o

at_agent: ${AGENT_OBJECTS}
	${CC} ${LDFLAGS} ${AGENT_OBJECTS} -o at_agent

at_agent.o: at_agent.c ../at.h at_agent.h
	${CC} ${CFLAGS} -o at_agent.o -c at_agent.c 

fsops.o: fsops.c ../at.h at_agent.h
	${CC} ${CFLAGS} -o fsops.o -c fsops.c

cpuops.o: cpuops.c ../at.h at_agent.h
	${CC} ${CFLAGS} -o cpuops.o -c cpuops.c

iodevops.o: iodevops.c ../at.h at_agent.h
	${CC} ${CFLAGS} -o iodevops.o -c iodevops.c

ifaceops.o: ifaceops.c ../at.h at_agent.h
	${CC} ${CFLAGS} -o ifaceops.o -c ifaceops.c

../cJSON.o: ../cJSON.c ../cJSON.h
	cd .. && make cJSON.o
    
clean:
	rm -f *.o at_agent core.*
