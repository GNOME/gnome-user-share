Summary: Gnome user file sharing
Name: gnome-user-share
Version: 0.3
Release: 1
License: GPL
Group: System Environment/Libraries
URL: http://www.gnome.org
Source0: %{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root
Requires: howl-libs >= 0.9.6
Requires: httpd
BuildRequires: GConf2-devel howl-devel pkgconfig
Prereq: GConf2

%description
gnome-user-share 

%prep
%setup -q

%build
%configure
make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT

export GCONF_DISABLE_MAKEFILE_SCHEMA_INSTALL=1
make install DESTDIR=$RPM_BUILD_ROOT
unset GCONF_DISABLE_MAKEFILE_SCHEMA_INSTALL

%clean
rm -rf $RPM_BUILD_ROOT

%post
export GCONF_CONFIG_SOURCE=`gconftool-2 --get-default-source`
SCHEMAS="desktop_gnome_file_sharing.schemas"
for S in $SCHEMAS; do
  gconftool-2 --makefile-install-rule %{_sysconfdir}/gconf/schemas/$S > /dev/null
done

%files
%defattr(-,root,root,-)
%doc README ChangeLog
%{_bindir}/*
%{_libexecdir}/*
%{_datadir}/gnome-user-share
%{_sysconfdir}/gconf/schemas/*

%changelog
* Fri Nov 26 2004 Alexander Larsson <alexl@redhat.com> - 0.3-1
- New version

* Thu Sep  9 2004 Alexander Larsson <alexl@redhat.com> - 0.2-1
- New version

* Wed Sep  8 2004 Alexander Larsson <alexl@redhat.com> - 0.1-1
- Initial Build

