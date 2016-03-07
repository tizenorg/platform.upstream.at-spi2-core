%define debug_package %{nil}
%bcond_with x

Name: at-spi2-core
Version: 2.16.1
Release: 0
Summary: Assistive Technology Service Provider Interface - D-Bus based implementation
License: LGPL-2.0+
Group: System/Libraries
Url: http://www.gnome.org/
Source: http://ftp.gnome.org/pub/GNOME/sources/at-spi2-core/2.16/%{name}-%{version}.tar.xz
Source1001:    %{name}.manifest
Requires:      dbus
BuildRequires: python-devel
BuildRequires: python-xml
BuildRequires: intltool
BuildRequires: dbus-devel
BuildRequires: glib2-devel
BuildRequires: gettext
BuildRequires: gtk-doc
%if %{with x}
BuildRequires: libX11-devel
BuildRequires: libXtst-devel
BuildRequires: libXi-devel
%endif
BuildRequires: pkgconfig(vconf)
BuildRequires: pkgconfig(appsvc)
BuildRequires: pkgconfig(dlog)
BuildRequires: pkgconfig(aul)

%description
AT-SPI is a general interface for applications to make use of the
accessibility toolkit. This version is based on dbus.

This package contains the AT-SPI registry daemon. It provides a
mechanism for all assistive technologies to discover and interact
with applications running on the desktop.

%package -n libatspi0
Summary: An Accessibility ToolKit -- Library
Group: System/Libraries

%description -n libatspi0
AT-SPI is a general interface for applications to make use of the
accessibility toolkit. This version is based on dbus.

%package -n typelib-1_0-Atspi-2_0
Summary: An Accessibility ToolKit -- Introspection bindings
Group: System/Libraries

%description -n typelib-1_0-Atspi-2_0
AT-SPI is a general interface for applications to make use of the
accessibility toolkit. This version is based on dbus.

This package provides the GObject Introspection bindings for the
libatspi library.

%package devel
Summary: Include Files and Libraries mandatory for Development
Group: Development/Libraries
Requires: %{name} = %{version}
Requires: libatspi0 = %{version}
Requires: typelib-1_0-Atspi-2_0 = %{version}

%description devel
This package contains all necessary include files and libraries needed
to develop applications that require these.

%prep
%setup -q
cp %{SOURCE1001} .

%build
%autogen --libexecdir=%{_libexecdir}/at-spi2 \
        --with-dbus-daemondir=%{_bindir} \
%if !%{with x}
        --disable-x11 \
%endif
        --disable-static
%__make %{?_smp_flags}

%install
rm -rf %{buildroot}
find %{buildroot} -name '*.la' -or -name '*.a' | xargs rm -f
%make_install
%find_lang %{name}
mkdir -p %{buildroot}/%{_datadir}/usr/share/license
cp -f %{_builddir}/%{buildsubdir}/COPYING %{buildroot}/%{_datadir}/usr/share/license/%{name}

%check

%clean
rm -fr %{buildroot}

%post -n libatspi0 -p /sbin/ldconfig

%postun -n libatspi0 -p /sbin/ldconfig

%files -f %{name}.lang
%manifest %{name}.manifest
%defattr(-,root,root)
%doc AUTHORS README
#%license COPYING
%{_libexecdir}/at-spi2/at-spi-bus-launcher
%{_libexecdir}/at-spi2/at-spi2-registryd
%config %{_sysconfdir}/at-spi2/accessibility.conf
%{_sysconfdir}/xdg/autostart/at-spi-dbus-bus.desktop
%{_datadir}/dbus-1/accessibility-services/org.a11y.atspi.Registry.service
%{_datadir}/dbus-1/services/org.a11y.Bus.service
%{_datadir}/usr/share/license/%{name}

%files -n libatspi0
%manifest %{name}.manifest
%defattr(-, root, root)
%{_libdir}/libatspi.so.0*

%files -n typelib-1_0-Atspi-2_0
%manifest %{name}.manifest
%defattr(-, root, root)

%files devel
%manifest %{name}.manifest
%defattr(-, root, root)
%{_includedir}/at-spi-2.0
%{_libdir}/libatspi.so
%{_libdir}/pkgconfig/atspi-2.pc

