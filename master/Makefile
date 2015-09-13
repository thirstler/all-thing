CC=gcc
CFLAGS:=-g -m64 -Wall -O1
LDFLAGS:=-g -lm -ljansson -lpthread -lpq -luuid

at_master: mstr_dataops.o mstr_listener.o at_master.o mstr_datasrv.o at_db.o
	${CC} ${LDFLAGS} mstr_datasrv.o mstr_dataops.o mstr_listener.o at_master.o ../ini.o at_db.o -o at_master
	
at_db.o: ../at.h at_master.h at_db.c
	${CC} ${MASTER_LDFLAGS} -o at_db.o -c at_db.c
	
mstr_dataops.o: mstr_dataops.c ../at.h at_master.h
	${CC} ${CFLAGS} -o mstr_dataops.o -c mstr_dataops.c
	
mstr_listener.o: mstr_listener.c ../at.h at_master.h
	${CC} ${CFLAGS} -o mstr_listener.o -c mstr_listener.c

mstr_datasrv.o: mstr_datasrv.c ../at.h at_master.h
	${CC} ${CFLAGS} -o mstr_datasrv.o -c mstr_datasrv.c

at_master.o: at_master.c ../at.h at_master.h
	${CC} ${CFLAGS} -o at_master.o -c at_master.c

at_agent.o: at_agent.c ../at.h at_master.h
	${CC} ${CFLAGS} -o at_agent.o -c at_agent.c
	
clean:
	rm -f *.o at_master core.*
	
