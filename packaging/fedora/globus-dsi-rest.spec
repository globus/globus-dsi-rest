Name:           globus-dsi-rest
%global _name %(tr - _ <<< %{name})
Version:	0.2
Release:        1%{?dist}
Vendor:		Globus Support
Summary:        GridFTP DSI REST Helper API

Group:          System Environment/Daemons
Source:		%{_name}-%{version}.tar.gz
License:        ASL 2.0
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires:  globus-gridftp-server-devel
BuildRequires:  globus-gridftp-server-progs
BuildRequires:  globus-gass-copy-progs
BuildRequires:  globus-common-devel
BuildRequires:  curl-devel
BuildRequires:  doxygen
BuildRequires:  jansson-devel
%if %{?fedora}%{!?fedora:0} >= 18 || %{?rhel}%{!?rhel:0} >= 6
BuildRequires:  perl-Test-Simple
%endif
BuildRequires:  perl-URI

%if %{?fedora}%{!?fedora:0} >= 19 || %{?rhel}%{!?rhel:0} >= 7
Requires:       openssl
Requires:       openssl-libs%{?_isa}
%endif
%if %{?fedora}%{!?fedora:0} < 19 && %{?rhel}%{!?rhel:0} < 7
Requires:       openssl%{?_isa}
%endif

%if %{?fedora}%{!?fedora:0} >= 19 || %{?rhel}%{!?rhel:0} >= 7
BuildRequires:  automake >= 1.11
BuildRequires:  autoconf >= 2.60
BuildRequires:  libtool >= 2.2
%endif
BuildRequires:  pkgconfig

BuildRequires:  openssl
%if 0%{?suse_version} > 0
BuildRequires: libtool
%else
BuildRequires: libtool-ltdl-devel
%endif

Requires:       globus-gridftp-server-progs

%package devel
Summary:        GridFTP DSI REST Helper API
Group:          System Environment/Daemons
Requires:	globus-gridftp-server-devel
Requires:	globus-common-devel
Requires:	curl-devel

%package doc
Summary:        GridFTP DSI REST Helper API Documentation
Group:          System Environment/Daemons

%description
The Globus Toolkit is an open source software toolkit used for building Grid
systems and applications. It is being developed by the Globus Alliance and
many others all over the world. A growing number of projects and companies are
using the Globus Toolkit to unlock the potential of grids for their cause.

The %{name} package contains:
GridFTP DSI REST Helper API

%description devel
The Globus Toolkit is an open source software toolkit used for building Grid
systems and applications. It is being developed by the Globus Alliance and
many others all over the world. A growing number of projects and companies are
using the Globus Toolkit to unlock the potential of grids for their cause.

The %{name}-devel package contains:
GridFTP DSI REST Helper API Development Files

%description doc
The Globus Toolkit is an open source software toolkit used for building Grid
systems and applications. It is being developed by the Globus Alliance and
many others all over the world. A growing number of projects and companies are
using the Globus Toolkit to unlock the potential of grids for their cause.

The %{name}-devel package contains:
GridFTP DSI REST Helper API Documentation

%prep

%setup -q -n %{_name}-%{version}

%build

%configure --disable-static --docdir=%{_docdir}/%{name} --includedir=%{_includedir}/globus --sysconfdir=%{_sysconfdir}/globus --enable-silent-rules -q

make %{?_smp_mflags}
%install
rm -rf $RPM_BUILD_ROOT

make DESTDIR=$RPM_BUILD_ROOT install

# Remove libtool .la files
rm -f $RPM_BUILD_ROOT%{_libdir}/*.la

%check
GLOBUS_HOSTNAME=localhost make check

%clean
rm -rf $RPM_BUILD_ROOT

%post
/sbin/ldconfig

%files
%defattr(-,root,root,-)
%{_libdir}/libglobus_dsi_rest.so.*

%files devel
%defattr(-,root,root,-)
%{_libdir}/libglobus_dsi_rest.so
%{_libdir}/pkgconfig/globus-dsi-rest.pc
%{_includedir}/globus/globus_dsi_rest.h
%doc %{_docdir}/globus-dsi-rest/GLOBUS_LICENSE

%files doc
%defattr(-,root,root,-)
%doc %{_mandir}/man3/globus*
%doc %{_docdir}/globus-dsi-rest/html/*

%changelog
* Tue Jun 21 2016 Globus Toolkit <support@globus.org> - 0.2-1
- Add dependency on perl-Test-Simple

* Wed Jun  8 2016 Globus Toolkit <support@globus.org> - 0.0-1
- Initial package
