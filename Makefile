CC=gcc
CFLAGS:=-g -m64 -Wall
LDFLAGS:=-g -lm -ljansson 
LDFLAGS_T:=$(LDFLAGS) -lpthread
DESTDIR:=
VER=0.2

all: at_agent at_master

at_master: mstr_dataops.o mstr_listener.o at_master.o ini.o
	${CC} ${LDFLAGS_T} mstr_dataops.o mstr_listener.o at_master.o ini.o -o at_master

at_agent: ini.o fsops.o cpuops.o iodevops.o ifaceops.o at_agent.o
	${CC} ${LDFLAGS} ini.o fsops.o cpuops.o iodevops.o ifaceops.o at_agent.o -o at_agent

mstr_dataops.o: mstr_dataops.c at.h
	${CC} ${CFLAGS} -o mstr_dataops.o -c mstr_dataops.c
	
mstr_listener.o: mstr_listener.c at.h
	${CC} ${CFLAGS} -o mstr_listener.o -c mstr_listener.c

at_master.o: at_master.c at.h
	${CC} ${CFLAGS} -o at_master.o -c at_master.c

at_agent.o: at_agent.c at.h
	${CC} ${CFLAGS} -o at_agent.o -c at_agent.c

fsops.o: fsops.c at.h
	${CC} ${CFLAGS} -o fsops.o -c fsops.c
	
cpuops.o: cpuops.c at.h
	${CC} ${CFLAGS} -o cpuops.o -c cpuops.c

iodevops.o: iodevops.c at.h
	${CC} ${CFLAGS} -o iodevops.o -c iodevops.c

ifaceops.o: ifaceops.c at.h
	${CC} ${CFLAGS} -o ifaceops.o -c ifaceops.c

ini.o: ini.c ini.h
	${CC} ${CFLAGS} -o ini.o -c ini.c

install: at_agent at_master

install-agent: at_agent
	[ -d /usr/sbin ] || mkdir /usr/sbin
	[ -d /etc ] || mkdir /etc
	/usr/bin/id allthing &> /dev/null || useradd -c "All Thing User" -s /sbin/nologin allthing
	install -s -groot -oallthing -m0700 ./at_agent ${DESTDIR}/usr/sbin/at_agent
	[ -f ${DESTDIR}/etc/allthing.conf ] || install -groot -oroot -m0640 ./config/allthing.conf ${DESTDIR}/etc/allthing.conf

install-master: at_master
	[ -d /usr/sbin ] || mkdir /usr/sbin
	[ -d /etc ] || mkdir /etc
	/usr/bin/id allthing &> /dev/null || useradd -c "All Thing User" -s /sbin/nologin allthing
	install -s -groot -oallthing -m0700 ./at_master ${DESTDIR}/usr/sbin/at_master
	[ -f ${DESTDIR}/etc/allthing.conf ] || install -groot -oroot -m0640 ./config/allthing.conf ${DESTDIR}/etc/allthing.conf

at_agent-tar:
	mkdir -p ./all-thing-agent-${VER}/config
	cp -a *.c *.h Makefile ./all-thing-agent-${VER}/
	cp -a config/allthing.conf ./all-thing-agent-${VER}/config/
	tar -czf all-thing-agent-${VER}.tar.gz all-thing-agent-${VER}
	rm -rf ./all-thing-agent-${VER}

at_master-tar:
	mkdir -p ./all-thing-master-${VER}/config
	cp -a *.c *.h Makefile ./all-thing-master-${VER}/
	cp -a config/allthing.conf ./all-thing-master-${VER}/config/
	tar -czf all-thing-master-${VER}.tar.gz all-thing-master-${VER}
	rm -rf ./all-thing-master-${VER}
		
clean:
	rm -f *.o at_agent at_master *.tar core.*
	
