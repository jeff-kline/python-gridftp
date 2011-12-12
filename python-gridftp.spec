%define name python-gridftp
%define version 1.3.0
%define unmangled_version 1.3.0
%define release 1

Summary: Python Globus GridFTP client bindings
Name: %{name}
Version: %{version}
Release: %{release}
Source0: %{name}-%{unmangled_version}.tar.gz
License: GPL
Group: Development/Libraries
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-buildroot
Prefix: %{_prefix}
Vendor: Scott Koranda, Jeff Kline <ldr-lsc@gravity.phys.uwm.edu>
Url: https://wiki.ligo.org/LDG/DASWG/LIGODataReplicator
BuildRequires: python-devel, python-setuptools, globus-ftp-client-devel >= 6.0
Requires: python, globus-ftp-client

%description
Python GridFTP wrappings for LIGO

%prep
%setup -n %{name}-%{unmangled_version}

%build
env CFLAGS="$RPM_OPT_FLAGS" python setup.py build

%install
python setup.py install -O1 --root=$RPM_BUILD_ROOT --record=INSTALLED_FILES

%clean
rm -rf $RPM_BUILD_ROOT

%files -f INSTALLED_FILES
%defattr(-,root,root)
