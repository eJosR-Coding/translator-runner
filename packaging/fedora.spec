Name:           translator-runner
Version:        0.1.0
Release:        1%{?dist}
Summary:        KRunner plugin to translate text via translate-shell
License:        GPL-3.0-or-later
URL:            https://github.com/eJosR-Coding/translator-runner
Source0:        %{url}/archive/v%{version}/%{name}-%{version}.tar.gz

BuildRequires:  cmake
BuildRequires:  gcc-c++
BuildRequires:  extra-cmake-modules
BuildRequires:  qt6-qtbase-devel
BuildRequires:  kf6-krunner-devel
BuildRequires:  kf6-kcoreaddons-devel
BuildRequires:  kf6-ki18n-devel
BuildRequires:  kf6-knotifications-devel

Requires:       translate-shell
Requires:       plasma-workspace >= 6.0

%description
A KRunner plugin that translates text inline via translate-shell.
Supports multiple target languages with syntax tr:<text> and tr-<lang>:<text>.

%prep
%autosetup

%build
%cmake
%cmake_build

%install
%cmake_install

%files
%license LICENSE
%doc README.md
%{_libdir}/qt6/plugins/kf6/krunner/translatorrunner.so
%{_datadir}/knotifications6/translatorrunner.notifyrc

%changelog
* Tue May 20 2026 eJosR-Coding <joseph.rodriguez.parco@outlook.com> - 0.1.0-1
- Initial release
