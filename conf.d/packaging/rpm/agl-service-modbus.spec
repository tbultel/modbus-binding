%define debug_package %{nil}
%define _modbus_datadir %{_datadir}/%{name}

Name: agl-service-modbus
Version: 1.0
Release: 1%{?dist}
Summary: Binding to serve an API connected to modbus hardware
Group:   Development/Libraries/C and C++
License: IRCL
URL: https://github.com/iotbzh/modbus-agl-binding
Source: %{name}-%{version}.tar.gz
Source2: %{name}.service
BuildRequires:  cmake
BuildRequires:  agl-cmake-apps-module
BuildRequires:  gcc-c++
BuildRequires:  pkgconfig(json-c)
BuildRequires:  agl-libmodbus-devel
BuildRequires:  pkgconfig(lua) >= 5.3
BuildRequires:  agl-app-framework-binder-devel
BuildRequires:  agl-libafb-helpers-devel
BuildRequires:  agl-libappcontroller-devel
BuildRequires:  pkgconfig(libsystemd) >= 222
Requires:       agl-app-framework-binder
Requires:		agl-libmodbus

%description
low can level

%package devel
Requires:       %{name} = %{version}
Provides:       pkgconfig(%{name}) = %{version}
Summary:        %{name} devel
%description devel
%summary

%package plugin-kingpigeon
Summary: plugin-kingpigeon subpackage
Group:          Development/Libraries/C and C++
%description plugin-kingpigeon
plugin-kingpigeon subpackage.
Requires:       %{name} = %{version}
Conflicts:		plugin-raymarine-anemometer
Summary:        Modbus plugin kingpigeon


%package plugin-raymarine-anemometer
Summary: plugin-raymarine-anemometer subpackage
Group:          Development/Libraries/C and C++
Requires:       %{name} = %{version}
%description plugin-raymarine-anemometer
plugin-raymarine-anemometer subpackage.
Conflicts:		plugin-kingpigeon
Summary:        Modbus plugin raymarine anemometer

%prep
%autosetup -p 1


%build
#hack because of libmodbus warning conversions
%define __global_compiler_flags -O2 -g -pipe -Wall -Wp,-D_FORTIFY_SOURCE=2 -Wp,-D_GLIBCXX_ASSERTIONS -fexceptions -fstack-protector-strong -grecord-gcc-switches %{_hardened_cflags} %{_annotated_cflags}
mkdir -p %{_target_platform}
pushd %{_target_platform}
%cmake -DVERSION=%{version} -DCMAKE_BUILD_TYPE=RELEASE -DCMAKE_INSTALL_PREFIX=%{_prefix} -DCONTROL_CONFIG_PATH=%{_sysconfdir}/%{name} ..
popd
%make_build -C %{_target_platform}

%install
%make_install -C %{_target_platform}
install -d %{?buildroot}%{_modbus_datadir}
mv %{?buildroot}%{_prefix}/%{name}/* %{?buildroot}%{_modbus_datadir}
install -d %{?buildroot}%{_userunitdir}
install -m 0644 %{SOURCE2} %{?buildroot}%{_userunitdir}
install -d %{?buildroot}%{_libdir}
ln -sf %{_modbus_datadir}/lib/afb-modbus.so %{?buildroot}%{_libdir}/libafb-modbus.so
#example file configuration
install -d %{?buildroot}/%{_sysconfdir}/%{name}
install -m 0644 %{?buildroot}/%{_modbus_datadir}/etc/control-modbus_kingpigeon-config.json %{?buildroot}/%{_sysconfdir}/%{name}/control-modbus.json

%post

%postun

%files
%dir %{_modbus_datadir}/bin
%dir %{_modbus_datadir}/var
%dir %{_modbus_datadir}/etc
%{_modbus_datadir}/htdocs
%{_modbus_datadir}/lib/afb-modbus.so
%{_modbus_datadir}/icon.png
%{_modbus_datadir}/config.xml
%{_userunitdir}/%{basename: %SOURCE2}
%{_sysconfdir}/%{name}/control-modbus.json
%{_libdir}/libafb-modbus.so

%files devel
%{_libdir}/pkgconfig/*.pc
%{_includedir}

%files plugin-kingpigeon
%{_modbus_datadir}/etc/control-modbus_kingpigeon-config.json
%{_modbus_datadir}/lib/plugins/kingpigeon.ctlso

%files plugin-raymarine-anemometer
%{_modbus_datadir}/etc/control-modbus_raymarine-anemometer-config.json
%{_modbus_datadir}/lib/plugins/raymarine-anemometer.ctlso

%changelog
* Wed Jan 8 2020 Clément Bénier <clement.benier@iot.bzh>
- Initial specfile
