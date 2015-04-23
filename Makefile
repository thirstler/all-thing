CJSONFLAGS:=
#ifdef ${CJSONFLAGS}
CJSON_OBJS:=cJSON.o
#endif
CC=gcc
CFLAGS:=-g -m64 -Wall -O1 ${CJSONFLAGS}
LDFLAGS:=-g -lm -ljansson -lpthread
DESTDIR:=
MASTER_LDFLAGS:=${LDFLAGS} -lpq
AGENT_TL_OBJECTS=agent/Makefile ini.o ${CJSON_OBJS}

all: at_agent at_master 

at_agent: ${AGENT_TL_OBJECTS}
	cd agent && CJSONFLAGS=${CJSONFLAGS} make

at_master: master/Makefile ini.o
	cd master && make

ini.o: ini.c ini.h
	${CC} ${CFLAGS} -o ini.o -c ini.c

cJSON.o: cJSON.c cJSON.h
	${CC} ${CFLAGS} -o cJSON.o -c cJSON.c

install: at_agent

install-agent: at_agent
	[ -d /usr/sbin ] || mkdir /usr/sbin
	[ -d /etc ] || mkdir /etc
	/usr/bin/id allthing &> /dev/null || useradd -c "All Thing User" -s /sbin/nologin allthing
	install -s -groot -oallthing -m0700 ./agent/at_agent ${DESTDIR}/usr/sbin/at_agent
	[ -f ${DESTDIR}/etc/allthing.conf ] || install -groot -oroot -m0640 ./config/allthing.conf ${DESTDIR}/etc/allthing.conf
	install -groot -oallthing -m755 ./scripts/at_agent.rc /etc/init.d/at_agent
	chkconfig --add /etc/init.d/at_agent

install-master: at_master
	[ -d /usr/sbin ] || mkdir /usr/sbin
	[ -d /etc ] || mkdir /etc
	/usr/bin/id allthing &> /dev/null || useradd -c "All Thing User" -s /sbin/nologin allthing
	install -s -groot -oallthing -m0700 ./master/at_master ${DESTDIR}/usr/sbin/at_master
	[ -f ${DESTDIR}/etc/allthing.conf ] || install -groot -oroot -m0640 ./config/allthing.conf ${DESTDIR}/etc/allthing.conf
	install -groot -oallthing -m755 ./scripts/at_master.rc /etc/init.d/at_master
	chkconfig --add /etc/init.d/at_master
	


# For creating tarballs for SRPM generation, increment with SPEC file
VER=0.8.3

srcrpms: at_agent-tar at_master-tar
	sed -i "s/^Version: .*$$/Version: ${VER}/" rpmspec/at_agent.spec
	sed -i "s/^Version: .*$$/Version: ${VER}/" rpmspec/at_master.spec
	cp -f rpmspec/at_agent.spec ~/rpmbuild/SPECS
	cp -f rpmspec/at_master.spec ~/rpmbuild/SPECS
	cp -f all-thing-agent-${VER}.tar.gz ~/rpmbuild/SOURCES
	cp -f all-thing-master-${VER}.tar.gz ~/rpmbuild/SOURCES
	cd ~/rpmbuild
	rpmbuild -bs rpmspec/at_agent.spec
	rpmbuild -bs rpmspec/at_master.spec
	

at_agent-tar:
	mkdir -p ./all-thing-agent-${VER}/config
	mkdir -p ./all-thing-agent-${VER}/scripts
	cp -a agent *.c *.h Makefile ./all-thing-agent-${VER}/
	cp -a config/allthing.conf ./all-thing-agent-${VER}/config/
	cp -a scripts/at_agent.rc ./all-thing-agent-${VER}/scripts/
	tar -czf all-thing-agent-${VER}.tar.gz all-thing-agent-${VER}
	rm -rf ./all-thing-agent-${VER}

at_master-tar:
	mkdir -p ./all-thing-master-${VER}/config
	mkdir -p ./all-thing-master-${VER}/scripts
	cp -a master *.c *.h Makefile ./all-thing-master-${VER}/
	cp -a config/allthing.conf ./all-thing-master-${VER}/config/
	cp -a scripts/at_master.rc ./all-thing-master-${VER}/scripts/
	tar -czf all-thing-master-${VER}.tar.gz all-thing-master-${VER}
	rm -rf ./all-thing-master-${VER}
		
clean:
	cd agent && make clean
	cd master && make clean
	rm -f *.o at_agent at_master *.tar *.tar.gz core.*
	
