Name: all-thing-master
Version: 0.8.6
Release: 4%{?dist}
Summary: All thing monitoring collector/server	
 
Group: Monitors		
License: GPLv3
URL: https://github.com/thirstler/all-thing
Source0: %{name}-%{version}.tar.gz

BuildRequires:	make gcc jansson-devel
Requires: glibc jansson all-thing-agent coreutils

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
	install -groot -oroot -m0755 /usr/share/allthing/at_master.rc /etc/rc.d/init.d/at_master
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
* Thu Sep 24 2015 <Jason Russler> jason.russler@gmail.com 0.8.8-1
- New build for v0.8.9
* Sat Sep 19 2015 <Jason Russler> jason.russler@gmail.com 0.8.8-1
- New build for v0.8.8
* Tue Sep 1 2015 <Jason Russler> jason.russler@gmail.com 0.8.7-1
- Fxed agent for RHEL6, no "availablememory" in /proc/meminfo
* Mon Aug 31 2015 <Jason Russler> jason.russler@gmail.com 0.8.6-4
- RPM spec updates for system systems
* Sun Aug 30 2015 <Jason Russler> jason.russler@gmail.com 0.8.6-3
- Updated install, forgot to rearrange configs
* Sun Aug 30 2015 <Jason Russler> jason.russler@gmail.com 0.8.6-2
- Busted spec
* Sun Aug 30 2015 <Jason Russler> jason.russler@gmail.com 0.8.6-1
- Added licencing.
