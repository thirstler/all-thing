Name: all-thing-agent
Version: 0.8.6
Release: 4
Summary: All thing monitoring agent	
 
Group: Monitors		
License: GPLv3
URL: https://github.com/thirstler/all-thing
Source0: %{name}-%{version}.tar.gz

BuildRequires:	make gcc
Requires: glibc /bin/cat /bin/mkdir /bin/cp procps-ng coreutils

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
	/bin/install -groot -oroot -m0755 /usr/share/allthing/at_agent.rc /etc/rc.d/init.d/at_agent
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
	service at_agent stop
	/sbin/chkconfig at_agent off
	/sbin/chkconfig --del at_agent
	/bin/rm /etc/rc.d/init.d/at_agent
fi

%changelog
* Mon Aug 31 2015 <Jason Russler> jason.russler@gmail.com 0.8.6-4
- RPM spec updates for systemd systems
* Sun Aug 30 2015 <Jason Russler> jason.russler@gmail.com 0.8.6-3
- Fixed config installation
* Sun Aug 30 2015 <Jason Russler> jason.russler@gmail.com 0.8.6-1
- Added licencing.
* Sun Aug 2 2015 <Jason Russler> jason.russler@gmail.com 0.8.5-1
- Updates to master, nothing for the agent
* Sun Jul 26 2015 <Jason Russler> jason.russler@gmail.com 0.8.4-1
- Fixed some issues with stat() on ISO9660 file systems
- Added selector for parsing meminfo since some of that is a little
  dynamic (1G huge pages not present on systems with <=1G mem is one case)
* Tue Oct 28 2014 <Jason Russler> jason.russler@gmail.com 0.8.1-1
- Updated agent to handle counter prefixes so that rates can be processed
  correctly on the other end
* Tue Oct 21 2014 <Jason Russler> jason.russler@gmail.com 0.8-2
- fixed busted agent RPM
* Sat Oct 18 2014 <Jason Russler> jason.russler@gmail.com 0.8-1
- Updated to all-thing version 0.8
* Sun Aug 31 2014 <Jason Russler> jason.russler@gmail.com 0.7-1
- Added init scripts
* Mon Aug 25 2014 <Jason Russler> jason.russler@gmail.com 0.6-1
- Started database backend work, many fixes added
* Mon Jun 2 2014 <Jason Russler> jason.russler@gmail.com 0-0.5.1
- Agent won't report inactive network or block devices
* Sun Jun 1 2014 <Jason Russler> jason.russler@gmail.com 0-0.5
- Working bare-bones agent
* Thu Feb 27 2014 <Jason Russler> jason.russler@gmail.com 0-0.2
- Initial RPM build
