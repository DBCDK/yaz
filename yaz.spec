Summary: Z39.50 Programs
Name: yaz
Version: 4.1.2
Release: 1
Requires: libxslt, gnutls, readline, libyaz4 = %{version}
License: BSD
Group: Applications/Internet
Vendor: Index Data ApS <info@indexdata.dk>
Source: yaz-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-root
%define TCPWRAPPER tcpd-devel
%if "%{_vendor}" == "redhat"
# Fedora requires tcp_wrappers-devel .
%define TCPWRAPPER tcp_wrappers
%endif
BuildRequires: pkgconfig, libxml2-devel, libxslt-devel, gnutls-devel, readline-devel, libicu-devel, %{TCPWRAPPER}
Packager: Adam Dickmeiss <adam@indexdata.dk>
URL: http://www.indexdata.com/yaz

%description
This package contains both a test-server and clients (normal & ssl)
for the ANSI/NISO Z39.50 protocol for Information Retrieval.

%package -n libyaz4
Summary: Z39.50 Library
Group: Libraries
Requires: libxslt, gnutls, libicu

%description -n libyaz4
YAZ is a library for the ANSI/NISO Z39.50 protocol for Information
Retrieval.

%post -n libyaz4 -p /sbin/ldconfig 
%postun -n libyaz4 -p /sbin/ldconfig 

%package -n libyaz4-devel
Summary: Z39.50 Library - development package
Group: Development/Libraries
Requires: libyaz4 = %{version}, libxml2-devel, libxslt-devel, libicu-devel
Conflicts: libyaz-devel

%description -n libyaz4-devel
Development libraries and includes for the libyaz package.

%package -n yaz-illclient
Summary: ILL client
Group: Applications/Communication
Requires: readline, libyaz4 = %{version}

%description -n yaz-illclient
yaz-illclient: an ISO ILL client.

%package -n yaz-icu
Summary: Command line utility for ICU utilities of YAZ
Group: Applications/Communication
Requires: libyaz4 = %{version}

%description -n yaz-icu
The yaz-icu program is a command-line based client which exposes the ICU
chain facility of YAZ.

%prep
%setup

%build

CFLAGS="$RPM_OPT_FLAGS" \
 ./configure --prefix=%{_prefix} --libdir=%{_libdir} --mandir=%{_mandir} \
	--enable-shared --enable-tcpd --with-xslt --with-gnutls --with-icu
make CFLAGS="$RPM_OPT_FLAGS"

%install
rm -fr ${RPM_BUILD_ROOT}
make prefix=${RPM_BUILD_ROOT}/%{_prefix} mandir=${RPM_BUILD_ROOT}/%{_mandir} \
	libdir=${RPM_BUILD_ROOT}/%{_libdir} install
rm ${RPM_BUILD_ROOT}/%{_libdir}/*.la

%clean
rm -fr ${RPM_BUILD_ROOT}

%files
%defattr(-,root,root)
%doc README LICENSE NEWS
%{_bindir}/yaz-client
%{_bindir}/yaz-ztest
%{_bindir}/zoomsh
%{_bindir}/yaz-marcdump
%{_bindir}/yaz-iconv
%{_bindir}/yaz-json-parse
%{_mandir}/man1/yaz-client.*
%{_mandir}/man1/yaz-json-parse.*
%{_mandir}/man8/yaz-ztest.*
%{_mandir}/man1/zoomsh.*
%{_mandir}/man1/yaz-marcdump.*
%{_mandir}/man1/yaz-iconv.*
%{_mandir}/man7/yaz-log.*
%{_mandir}/man7/bib1-attr.*

%files -n libyaz4
%defattr(-,root,root)
%{_libdir}/*.so.*

%files -n libyaz4-devel
%defattr(-,root,root)
%{_bindir}/yaz-config
%{_bindir}/yaz-asncomp
%{_includedir}/yaz
%{_libdir}/pkgconfig/yaz.pc
%{_libdir}/*.so
%{_libdir}/*.a
%{_datadir}/aclocal/yaz.m4
%{_mandir}/man1/yaz-asncomp.*
%{_mandir}/man7/yaz.*
%{_mandir}/man?/yaz-config.*
%{_datadir}/doc/yaz
%{_datadir}/yaz

%files -n yaz-illclient
%defattr(-,root,root)
%{_bindir}/yaz-illclient
%{_mandir}/man1/yaz-illclient.*

%files -n yaz-icu
%defattr(-,root,root)
%{_bindir}/yaz-icu
%{_mandir}/man1/yaz-icu.*