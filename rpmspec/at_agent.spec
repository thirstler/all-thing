Name: all-thing-agent
Version: 0.8.5
Release: 1%{?dist}
Summary: All thing monitoring agent	
 
Group: Monitors		
License: GPLv3
URL: https://github.com/thirstler/all-thing
Source0: %{name}-%{version}.tar.gz

BuildRequires:	make gcc
Requires: glibc jansson

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
mkdir -p %{buildroot}/etc/init.d
cp agent/at_agent %{buildroot}/usr/sbin/
cp config/allthing.conf %{buildroot}/etc/
cp scripts/at_agent.rc %{buildroot}/etc/init.d/at_agent

%files
%config(noreplace) %attr(640 root root) /etc/allthing.conf
%attr(700 root root) /usr/sbin/at_agent
%attr(755 root root) /etc/init.d/at_agent

%post
/usr/bin/id allthing &> /dev/null || useradd -c "All Thing User" -s /sbin/nologin -M allthing
chkconfig --add /etc/init.d/at_agent

%changelog
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
