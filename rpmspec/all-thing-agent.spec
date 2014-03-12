Name: all-thing-agent
Version: 0.2
Release: 1%{?dist}
Summary: All thing monitoring agent	
 
Group: Monitors		
License: GPLv3
URL: https://github.com/thirstler/all-thing
Source0: %{name}-%{version}.tar.gz

BuildRequires:	make gcc
Requires: glibc

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
cp at_agent %{buildroot}/usr/sbin/
cp config/allthing.conf %{buildroot}/etc/


%files
%config(noreplace) %attr(640 root root) /etc/allthing.conf
%attr(700 root root) /usr/sbin/at_agent

%post
/usr/bin/id allthing &> /dev/null || useradd -c "All Thing User" -s /sbin/nologin allthing



%changelog
* Thu Feb 27 2014 <Jason Russler> jason.russler@gmail.com 0-0.2
- Initial RPM build
