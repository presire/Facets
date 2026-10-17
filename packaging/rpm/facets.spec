# facets (KWin 6デスクトップ切替アニメーション) のRPM仕様
#
# 1つのソースからWayland版KWinとX11版KWinの両方へビルドし、
# それぞれ別のバイナリパッケージへ振り分ける:
#   facets-common: 翻訳カタログ + ドキュメント (アーキテクチャ非依存の内容)
#   facets-wayland: libkwin.so系 (Waylandセッション) のプラグイン
#   facets-x11: libkwin-x11.so系 (X11セッション) のプラグイン
#
# コンパイル済みKWinエフェクトはビルド時とまったく同じKWinにしかロードできない。
# そのため実行時依存は、ビルドに使った-develパッケージのVERSIONを解析して
# 厳密一致 (=) でピン留めする。
#
# ビルドはpackaging/rpm/build-rpm.shから呼ばれる (ドライバスクリプト参照)。

# ---------------------------------------------------------------------------
# ディストリビューションごとのパッケージ名とマクロ
# ---------------------------------------------------------------------------
# ECMのkcoreaddons_add_plugin()はKDE_INSTALL_PLUGINDIR配下へ配置し、
# その既定値はKDE_INSTALL_QTPLUGINDIRと同じ。ここではシステムのQt
# プラグインディレクトリを明示する。
%if 0%{?fedora}
%global facets_qtplugindir  %{_qt6_plugindir}
%global kwin_wayland_pkg    kwin-libs
%global kwin_x11_pkg        kwin-x11-libs
%global kwin_devel_pkg      kwin-devel
%global kwin_x11_devel_pkg  kwin-x11-devel
%else
# openSUSEの_qt6_pluginsdirと_kf6_plugindirはいずれもlib64/qt6/pluginsを指す
%global facets_qtplugindir  %{_kf6_plugindir}
%global kwin_wayland_pkg    kwin6
%global kwin_x11_pkg        kwin6-x11
%global kwin_devel_pkg      kwin6-devel
%global kwin_x11_devel_pkg  kwin6-x11-devel
%endif

# ビルド時のKWin開発パッケージのVERSIONを実行時依存のピンに使う
%global kwin_wayland_version  %(rpm -q --qf '%%{VERSION}' %{kwin_devel_pkg} 2>/dev/null)
%global kwin_x11_version      %(rpm -q --qf '%%{VERSION}' %{kwin_x11_devel_pkg} 2>/dev/null)

Name:           facets
Version:        0.1.0
Release:        1%{?dist}
Summary:        KWin effect that switches virtual desktops with a 3D animation
License:        GPL-2.0-or-later
# アップストリームURLは未確定のため省略する
Source0:        %{name}-%{version}.tar.gz

# ネイティブプラグイン (facets.so, kwin_facets_config.so) をビルドする
BuildRequires:  cmake >= 3.16
BuildRequires:  gcc-c++

%if 0%{?fedora}
BuildRequires:  ninja-build
BuildRequires:  gettext
BuildRequires:  extra-cmake-modules
BuildRequires:  kwin-devel
BuildRequires:  kwin-x11-devel
BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-qtdeclarative-devel
BuildRequires:  kf6-kcmutils-devel
BuildRequires:  kf6-kconfig-devel
BuildRequires:  kf6-kconfigwidgets-devel
BuildRequires:  kf6-kcoreaddons-devel
BuildRequires:  kf6-ki18n-devel
BuildRequires:  kf6-kwidgetsaddons-devel
BuildRequires:  kf6-kwindowsystem-devel
BuildRequires:  kdecoration-devel
BuildRequires:  libepoxy-devel
BuildRequires:  libdrm-devel
BuildRequires:  wayland-devel
%else
BuildRequires:  ninja
BuildRequires:  gettext-tools
BuildRequires:  kf6-extra-cmake-modules
BuildRequires:  kwin6-devel
BuildRequires:  kwin6-x11-devel
BuildRequires:  qt6-base-devel
BuildRequires:  qt6-quick-devel
BuildRequires:  kf6-kcmutils-devel
BuildRequires:  kf6-kconfig-devel
BuildRequires:  kf6-kconfigwidgets-devel
BuildRequires:  kf6-kcoreaddons-devel
BuildRequires:  kf6-ki18n-devel
BuildRequires:  kf6-kwidgetsaddons-devel
BuildRequires:  kf6-kwindowsystem-devel
%endif

%description
Facets replaces the default virtual desktop switch with a short 3D
transition (Cube, Flip, or Deck). It is a KWin desktop effect for
Plasma 6 and works on both the Wayland and X11 sessions.

The effect and its settings dialog are compiled per KWin flavor, so the
plugins are split into the facets-wayland and facets-x11 plugin packages.
This base package holds only the shared, architecture-independent data
(the translation catalog and the documentation).

# ---------------------------------------------------------------------------
# 共通 (翻訳 + ドキュメント)
# ---------------------------------------------------------------------------
# アーキテクチャ非依存の内容だが、同一spec内にアーキテクチャ依存サブパッケージがあるため
# RPMの制約上noarchにはできない。
%package -n facets-common
Summary:        Shared translations and documentation for the Facets KWin effect

%description -n facets-common
Translation catalogs and documentation shared by the facets-wayland and
facets-x11 plugin packages. Install this package together with exactly one
of the plugin packages.

%files -n facets-common
%license LICENSE
%doc README.md README_JP.md
%lang(ja) %{_datadir}/locale/ja/LC_MESSAGES/kwin_effect_facets.mo

# ---------------------------------------------------------------------------
# Wayland版 (名前空間kwin, libkwin.so)
# ---------------------------------------------------------------------------
%package -n facets-wayland
Summary:        Facets KWin effect plugins for the Wayland session
Requires:       facets-common = %{version}-%{release}
# コンパイル済みエフェクトはビルド時と同一バージョンのKWinでのみロードできる
Requires:       %{kwin_wayland_pkg} = %{kwin_wayland_version}

%description -n facets-wayland
The Facets virtual desktop switching animation for the Wayland session.
It is built against the exact KWin version reported by %{kwin_devel_pkg}
at build time and therefore requires that same version at runtime.

%files -n facets-wayland
%{facets_qtplugindir}/kwin/effects/plugins/facets.so
%{facets_qtplugindir}/kwin/effects/configs/kwin_facets_config.so

# ---------------------------------------------------------------------------
# X11版 (名前空間kwin-x11, libkwin-x11.so)
# ---------------------------------------------------------------------------
%package -n facets-x11
Summary:        Facets KWin effect plugins for the X11 session
Requires:       facets-common = %{version}-%{release}
# コンパイル済みエフェクトはビルド時と同一バージョンのKWinでのみロードできる
Requires:       %{kwin_x11_pkg} = %{kwin_x11_version}

%description -n facets-x11
The Facets virtual desktop switching animation for the X11 session.
It is built against the exact KWin version reported by %{kwin_x11_devel_pkg}
at build time and therefore requires that same version at runtime.

%files -n facets-x11
%{facets_qtplugindir}/kwin-x11/effects/plugins/facets.so
%{facets_qtplugindir}/kwin-x11/effects/configs/kwin_facets_config.so

# ---------------------------------------------------------------------------
# ビルド
# ---------------------------------------------------------------------------
%prep
%autosetup

%build
# 2つのフレーバを別々のビルドディレクトリで構成し、両方をビルドする
for flavor in wayland x11; do
    cmake -S cpp -B "build-${flavor}" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=%{_prefix} \
        -DKDE_INSTALL_PLUGINDIR=%{facets_qtplugindir} \
        -DKDE_INSTALL_QTPLUGINDIR=%{facets_qtplugindir} \
        -DKDE_INSTALL_LOCALEDIR=%{_datadir}/locale \
        -DKWIN_FLAVOR="${flavor}"
    cmake --build "build-${flavor}" --parallel
done

%install
# 両フレーバを同じbuildrootへインストールする (.moは共通のため重複する)
for flavor in wayland x11; do
    DESTDIR=%{buildroot} cmake --install "build-${flavor}"
done

%changelog
* Sat Oct 17 2026 Facets Developers <noreply@example.com> - 0.1.0-1
- Initial RPM packaging
