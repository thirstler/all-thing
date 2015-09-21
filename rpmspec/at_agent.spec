Name: all-thing-agent
Version: 0.8.6
Release: 4%{?dist}
Summary: All thing monitoring agent	
 
Group: Monitors		
License: GPLv3
URL: https://github.com/thirstler/all-thing
Source0: %{name}-%{version}.tar.gz

BuildRequires:	make gcc
Requires: glibc coreutils

%description
Monitoring agent for the All-Thing HPC monitoring system

%prep
%setup -q

%build
make at_agent

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}/usr/sbin
mkdir -p %{buildroot}/etc
mkdir -p %{buildroot}/etc/allthing/
mkdir -p %{buildroot}/usr/share/allthing/
cp agent/at_agent %{buildroot}/usr/sbin/
cp config/allthing.conf %{buildroot}/etc/allthing/
cp config/agent.in %{buildroot}/etc/allthing/agent.conf
cp scripts/at_agent.rc %{buildroot}/usr/share/allthing/at_agent.rc
cp scripts/at_agent.service %{buildroot}/usr/share/allthing/at_agent.service

%files
%config(noreplace) %attr(640 root root) /etc/allthing/allthing.conf
%config(noreplace) %attr(640 root root) /etc/allthing/agent.conf
%attr(700 root root) /usr/sbin/at_agent
%attr(644 root root) /usr/share/allthing/at_agent.rc
%attr(644 root root) /usr/share/allthing/at_agent.service

%post
/usr/bin/id allthing &> /dev/null || useradd -c "All Thing User" -s /sbin/nologin -M allthing
if [ -f /etc/allthing.conf ]; then
    echo "reinstalling config, copy old config to /etc/allthing/old.conf"
    /bin/mv /etc/allthing.conf /etc/allthing/old.conf
    echo "refer to old config to recreate new (sorry)"
fi

##
# Install the service
if [[ "$(pgrep ^systemd$)" == "1" ]]; then
	/bin/cp /usr/share/allthing/at_agent.service /usr/lib/systemd/system/at_agent.service
	/bin/systemctl daemon-reload
	/bin/systemctl disable at_agent
else
	install -groot -oroot -m0755 /usr/share/allthing/at_agent.rc /etc/rc.d/init.d/at_agent
	/sbin/chkconfig --add at_agent
	/sbin/chkconfig at_agent off
fi

%preun
if [[ "$(pgrep ^systemd$)" == "1" ]]; then
	/bin/systemctl stop at_agent.service
	/bin/systemctl disable at_agent.service
	/bin/rm /usr/lib/systemd/system/at_agent.service
	/bin/systemctl daemon-reload
else
	/usr/sbin/service at_agent stop
	/sbin/chkconfig at_agent off
	/sbin/chkconfig --del at_agent
	/bin/rm /etc/rc.d/init.d/at_agent
fi

%changelog
* Sat Sep 19 2015 <Jason Russler> jason.russler@gmail.com 0.8.8-1
- New build for v8.8
* Tue Sep 1 2015 <Jason Russler> jason.russler@gmail.com 0.8.7-1
- Fxed agent for RHEL6, no "availablememory" in /proc/meminfo
* Mon Aug 31 2015 <Jason Russler> jason.russler@gmail.com 0.8.6-4
- RPM spec updates for systemd systems
* Sun Aug 30 2015 <Jason Russler> jason.russler@gmail.com 0.8.6-3
- Fixed config installation
* Sun Aug 30 2015 <Jason Russler> jason.russler@gmail.com 0.8.6-1
- Added licencing.
