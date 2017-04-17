Name:           globus-dsi-rest
%global _name %(tr - _ <<< %{name})
Version:	0.30
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
* Mon Apr 17 2017 Globus Toolkit <support@globus.org> - 0.30-1
- Fix memory buffer misuse

* Thu Apr 13 2017 Globus Toolkit <support@globus.org> - 0.29-1
- Add multiple-block data writer
- Fix documentation typos

* Wed Mar 29 2017 Globus Toolkit <support@globus.org> - 0.28-1
- Add multipart reader
- Increase JSON parser error string verbosity to help debug
  API calls that sometimes return non-JSON in an error situation

* Wed Mar 22 2017 Globus Toolkit <support@globus.org> - 0.27-1
- Fix hang when writing a 0 byte range.

* Fri Mar 17 2017 Globus Toolkit <support@globus.org> - 0.26-1
- Don't send an empty buffer at end of file in read_gridft_op

* Mon Mar 13 2017 Globus Toolkit <support@globus.org> - 0.25-1
- Use gridftp server recommended number of outstanding buffers
- Fix format of json log messsage
- Don't run gridftp server fork tests as root

* Wed Jan 11 2017 Globus Toolkit <support@globus.org> - 0.23-1
- Allow NULL key or value in URI parameters

* Mon Dec 19 2016 Globus Toolkit <support@globus.org> - 0.22-1
- Ensure printable log level string is not null.
- Leak in read_json
- Prefer timeout error over libcurl aborted by callback error

* Wed Dec 14 2016 Globus Toolkit <support@globus.org> - 0.21-1
- Favor timeout error over libcurl aborted error

* Wed Nov 23 2016 Globus Toolkit <support@globus.org> - 0.20-1
- Add curl cookie/ssl/dns resource sharing between handles

* Thu Nov 03 2016 Globus Toolkit <support@globus.org> - 0.18-1
- Improve diagnostics

* Fri Aug 19 2016 Globus Toolkit <support@globus.org> - 0.17-1
- Fix some logging issues

* Tue Jul 26 2016 Globus Toolkit <support@globus.org> - 0.16-1
- Add eof flag to gridftp_op_arg to pass back EOF state to requester
- Handle read until EOF in gridftp_op handling

* Tue Jul 19 2016 Globus Toolkit <support@globus.org> - 0.15-1
- Track amounts read/written in response callback

* Mon Jul 18 2016 Globus Toolkit <support@globus.org> - 0.14-1
- Add globus_dsi_rest_error_is_retryable()

* Fri Jul 15 2016 Globus Toolkit <support@globus.org> - 0.13-1
- Test case fixes

* Thu Jul 14 2016 Globus Toolkit <support@globus.org> - 0.12-1
- Fix bugs in partial read gridftp_op
- Propagate error from request instead of allowing curl error code
  to override it.

* Wed Jul 13 2016 Globus Toolkit <support@globus.org> - 0.11-1
- Add handle cache
- Add support for partial reads from gridftp_op to write to http server

* Tue Jul 12 2016 Globus Toolkit <support@globus.org> - 0.10-1
- Add new writer for sending data from multiple write callbacks,
  optionally adding multipart mime boundaries

* Thu Jun 23 2016 Globus Toolkit <support@globus.org> - 0.8-1
- Add TRACE level data on upload
- Explicit jansson flags
- When using LIBTOOL in tests, dlopen library

* Wed Jun 22 2016 Globus Toolkit <support@globus.org> - 0.7-1
- Add valgrind hook to tests
- Change argument to globus_dsi_rest_*_gridftp_op
- Move GridFTP range handling to DSI's responsibility
- Fix documentation of globus_dsi_rest_write_block_arg_t

* Tue Jun 21 2016 Globus Toolkit <support@globus.org> - 0.5-1
- Add dependency on perl-Test-Simple
- Add GLOBUS_HOSTNAME=localhost to test environment
- Add globus_i_dsi_rest_request_cleanup()
- Wait for write callbacks to gridftp to arrive before completing request
- Leak fixes in tests


* Wed Jun  8 2016 Globus Toolkit <support@globus.org> - 0.0-1
- Initial package
