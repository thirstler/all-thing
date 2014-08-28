Name: all-thing-master
Version: 0.6
Release: 1%{?dist}
Summary: All thing monitoring collector/server	
 
Group: Monitors		
License: GPLv3
URL: https://github.com/thirstler/all-thing
Source0: %{name}-%{version}.tar.gz

BuildRequires:	make gcc jansson-devel
Requires: glibc jansson

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
cp at_master %{buildroot}/usr/sbin/
cp config/allthing.conf %{buildroot}/etc/

%files
%config(noreplace) %attr(640 root root) /etc/allthing.conf
%attr(700 root root) /usr/sbin/at_master

%post
/usr/bin/id allthing &> /dev/null || useradd -c "All Thing User" -s /sbin/nologin -M allthing

%changelog
* Mon Aug 25 2014 <Jason Russler> jason.russler@gmail.com 0.6-1
- Started database backend work, many fixes added
* Sun Jun 2 2014 <Jason Russler> jason.russler@gmail.com 0-0.5.1
- Agent won't report inactive network or block devices
* Sun Jun 1 2014 <Jason Russler> jason.russler@gmail.com 0-0.5
- Working bare-bones master