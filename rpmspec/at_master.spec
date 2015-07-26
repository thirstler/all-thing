Name: all-thing-master
Version: 0.8.4
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
mkdir -p %{buildroot}/etc/init.d
cp master/at_master %{buildroot}/usr/sbin/
cp config/allthing.conf %{buildroot}/etc/
cp scripts/at_master.rc %{buildroot}/etc/init.d/at_master

%files
%config(noreplace) %attr(640 root root) /etc/allthing.conf
%attr(700 root root) /usr/sbin/at_master
%attr(755 root root) /etc/init.d/at_master

%post
/usr/bin/id allthing &> /dev/null || useradd -c "All Thing User" -s /sbin/nologin -M allthing
chkconfig --add at_master

%changelog
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
