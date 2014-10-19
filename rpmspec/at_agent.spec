Name: all-thing-agent
Version: 0.7
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
cp at_agent %{buildroot}/usr/sbin/
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
* Sat Aug 31 2014 <Jason Russler> jason.russler@gmail.com 0.7-1
- Added init scripts
* Mon Aug 25 2014 <Jason Russler> jason.russler@gmail.com 0.6-1
- Started database backend work, many fixes added
* Sun Jun 2 2014 <Jason Russler> jason.russler@gmail.com 0-0.5.1
- Agent won't report inactive network or block devices
* Sun Jun 1 2014 <Jason Russler> jason.russler@gmail.com 0-0.5
- Working bare-bones agent
* Thu Feb 27 2014 <Jason Russler> jason.russler@gmail.com 0-0.2
- Initial RPM build