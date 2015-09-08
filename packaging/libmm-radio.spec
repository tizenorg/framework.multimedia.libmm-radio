Name:       libmm-radio
Summary:    Multimedia Framework Radio Library
Version:    0.2.15
Release:    0
Group:      System/Libraries
License:    Apache-2.0
Source0:    %{name}-%{version}.tar.gz
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
BuildRequires:  audio-session-manager-devel
BuildRequires:  pkgconfig(mm-common)
BuildRequires:  pkgconfig(mm-log)
BuildRequires:  pkgconfig(mm-session)
BuildRequires:  pkgconfig(mm-sound)
BuildRequires:  pkgconfig(gstreamer-0.10)
BuildRequires:  pkgconfig(gstreamer-plugins-base-0.10)

%description
Description: Multimedia Framework Radio Library


%package devel
Summary:    Multimedia Framework Radio Library (DEV)
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}

%description devel
Description: Multimedia Framework Radio Library (DEV)

%prep
%setup -q

%build
./autogen.sh
CFLAGS=" %{optflags} -Wall -DEXPORT_API=\"__attribute__((visibility(\\\"default\\\")))\" "; export CFLAGS;
%configure \
%ifarch i586
--enable-emulator \
%endif
--disable-static --prefix=%{_prefix}



make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}/usr/share/license
cp LICENSE.APLv2 %{buildroot}/usr/share/license/%{name}
%make_install


%post -p /sbin/ldconfig

%postun  -p /sbin/ldconfig


%files
%manifest libmm-radio.manifest
%defattr(-,root,root,-)
%{_libdir}/libmmfradio.so.*
%{_bindir}/mm_radio_testsuite
/usr/share/license/%{name}


%files devel
%defattr(-,root,root,-)
%{_libdir}/libmmfradio.so
%{_libdir}/pkgconfig/mm-radio.pc
%{_includedir}/mmf/mm_radio.h
