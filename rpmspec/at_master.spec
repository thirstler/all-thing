Name: all-thing-master
Version: 0.8.6
Release: 4
Summary: All thing monitoring collector/server	
 
Group: Monitors		
License: GPLv3
URL: https://github.com/thirstler/all-thing
Source0: %{name}-%{version}.tar.gz

BuildRequires:	make gcc jansson-devel
Requires: glibc jansson all-thing-agent coreutils procps-ng

%description
Monitoring data collection agent and data server.

%prep
%setup -q

%build
make at_master

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}/usr/sbin
mkdir -p %{buildroot}/etc
mkdir -p %{buildroot}/etc/allthing
mkdir -p %{buildroot}/usr/share/allthing
cp master/at_master %{buildroot}/usr/sbin/
cp config/master.in %{buildroot}/etc/allthing/master.conf
cp scripts/at_master.rc %{buildroot}/usr/share/allthing/at_master.rc
cp scripts/at_master.service %{buildroot}/usr/share/allthing/at_master.service

%files
%config(noreplace) %attr(640 root root) /etc/allthing/master.conf
%attr(700 root root) /usr/sbin/at_master
%attr(644 root root) /usr/share/allthing/at_master.rc
%attr(644 root root) /usr/share/allthing/at_master.service

%post
/usr/bin/id allthing &> /dev/null || useradd -c "All Thing User" -s /sbin/nologin -M allthing
if [ -f /etc/allthing.conf ]; then
    echo "reinstalling config, copy old config to /etc/allthing/oldthing.conf"
    mv /etc/allthing.conf /etc/allthing/oldthing.conf
    echo "refer to old config to recreate new (sorry)"
fi

##
# Install the service
if [[ "$(pgrep ^systemd$)" == "1" ]]; then
	/bin/cp /usr/share/allthing/at_master.service /usr/lib/systemd/system/at_master.service
	/bin/systemctl daemon-reload
	/bin/systemctl disable at_master
else
	/bin/install -groot -oroot -m0755 /usr/share/allthing/at_master.rc /etc/rc.d/init.d/at_master
	/sbin/chkconfig --add at_master
	/sbin/chkconfig at_master off
fi

%preun
if [[ "$(pgrep ^systemd$)" == "1" ]]; then
    /bin/systemctl stop at_master.service
	/bin/systemctl disable at_master.service
	/bin/rm /usr/lib/systemd/system/at_master.service
	/bin/systemctl daemon-reload
else
    /usr/sbin/service at_master stop
	/sbin/chkconfig at_master off
	/sbin/chkconfig --del at_master
	/bin/rm /etc/rc.d/init.d/at_master
fi

%changelog
* Mon Aug 31 2015 <Jason Russler> jason.russler@gmail.com 0.8.6-4
- RPM spec updates for system systems
* Sun Aug 30 2015 <Jason Russler> jason.russler@gmail.com 0.8.6-3
- Updated install, forgot to rearrange configs
* Sun Aug 30 2015 <Jason Russler> jason.russler@gmail.com 0.8.6-2
- Busted spec
* Sun Aug 30 2015 <Jason Russler> jason.russler@gmail.com 0.8.6-1
- Added licencing.
* Sun Aug 2 2015 <Jason Russler> jason.russler@gmail.com 0.8.5-1
- Globalize sockets so that they can be closed from the master thread while
  the clients are blocked waiting for I/O
* Sun Jul 26 2015 <Jason Russler> jason.russler@gmail.com 0.8.4-1
- Clean up
* Tue Oct 28 2014 <Jason Russler> jason.russler@gmail.com 0.8.1-1
- Updated system to processes rate informantion differently. Rates will process
  in the presence of a flag in the json label. Rate data is now placed in the
  root of JSON query result objects.
* Sat Oct 18 2014 <Jason Russler> jason.russler@gmail.com 0.8-1
- Updated to all-thing version 0.8
* Sun Aug 31 2014 <Jason Russler> jason.russler@gmail.com 0.7-1
- Added init scripts contined database work
* Mon Aug 25 2014 <Jason Russler> jason.russler@gmail.com 0.6-1
- Started database backend work, many fixes added
* Mon Jun 2 2014 <Jason Russler> jason.russler@gmail.com 0-0.5.1
- Agent won't report inactive network or block devices
* Sun Jun 1 2014 <Jason Russler> jason.russler@gmail.com 0-0.5
- Working bare-bones master
