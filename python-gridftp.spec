Summary: Python GridFTP wrappings for LIGO
Vendor: UWM LIGO Group
Name: python-gridftp
Packager: Jeff Kline <kline@gravity.phys.uwm.edu>
License: GPL
Version: 1.3.0
Release: 1
URL: http://www.lsc-group.phys.uwm.edu
Group: Applications/Internet

Source0: %{name}-%{version}.tar.gz

BuildRoot: %{_tmppath}/%{name}-%{version}-buildroot

BuildRequires: python >= 2.6.6, python-devel >= 2.6.6, globus-ftp-client-devel >= 6.0, globus-gass-transfer-devel >= 4.3, globus-usage-devel >= 1.4

Requires: python >= 2.6.6, globus-ftp-client >= 6.0,  globus-gass-transfer >= 4.3, globus-usage >= 1.4


AutoReq: no
AutoProv: no

%define debug_package %{nil}

%description
Python GridFTP wrappings for LIGO

%prep
%setup
%{__python} setup.py build

%install
rm -rf $RPM_BUILD_ROOT
%{__python} setup.py install --skip-build --root $RPM_BUILD_ROOT

%clean
rm -rf $RPM_BUILD_ROOT

%post

%files 
%defattr(-,root,root)
%{python_sitearch}

%changelog
* Fri Dec 02 2011 Scott Koranda <skoranda@gravity.phys.uwm.edu> - 1.3.0-1
- Support for natively packaged Globus 

* Wed Feb 23 2011 Scott Koranda <skoranda@gravity.phys.uwm.edu> - 1.2.0-1
- Support for popen functionality 

* Mon Oct 19 2009 Scott Koranda <skoranda@gravity.phys.uwm.edu> - 1.1.0-1
- Support for 'exists' and better destruction of handles. 

* Mon Jun 08 2009 Scott Koranda <skoranda@gravity.phys.uwm.edu> - 1.0.0-1
- Initial release
