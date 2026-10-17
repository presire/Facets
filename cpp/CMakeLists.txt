# facets (KWin 6デスクトップ切替アニメーション)
#
# 1つのツリーから Wayland版KWinとX11版KWinの両方へビルドできるようにする
# [KWIN_FLAVOR]キャッシュ変数で切替える
#   wayland (既定) -> KWin   / libkwin.so   / 名前空間kwin
#   x11            -> KWinX11 / libkwin-x11.so / 名前空間kwin-x11
# サブディレクトリ (effect / kcm) は[KWIN_PACKAGE], [KWIN_TARGET], [KWIN_NS]を参照する

cmake_minimum_required(VERSION 3.16)

# KWinのプラグインidや翻訳ドメインを決めるために使う
project(facets VERSION 0.1.0 LANGUAGES CXX)

# KWinのヘッダがC++20のdefaulted比較演算子を使うため
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# フレーバ選択 (キャッシュ変数)
# ---------------------------------------------------------------------------
set(KWIN_FLAVOR "wayland" CACHE STRING "対象のKWinフレーバ (wayland または x11)")
set_property(CACHE KWIN_FLAVOR PROPERTY STRINGS wayland x11)

if(KWIN_FLAVOR STREQUAL "wayland")
    # Wayland版KWinのパッケージ名とリンク先ターゲット
    set(KWIN_PACKAGE KWin)
    set(KWIN_TARGET KWin::kwin)
    # プラグイン配置名前空間 (kwin/effects/plugins)
    set(KWIN_NS kwin)
elseif(KWIN_FLAVOR STREQUAL "x11")
    # X11版KWinのパッケージ名とリンク先ターゲット
    # ターゲット名はKWinX11::kwin ("kwin-x11"ではない)ので注意
    set(KWIN_PACKAGE KWinX11)
    set(KWIN_TARGET KWinX11::kwin)
    # プラグイン配置名前空間 (kwin-x11/effects/plugins)
    set(KWIN_NS kwin-x11)
else()
    message(FATAL_ERROR "KWIN_FLAVORは、wayland または x11を指定してください (現在: ${KWIN_FLAVOR})")
endif()

message(STATUS "facets: フレーバ=${KWIN_FLAVOR} パッケージ=${KWIN_PACKAGE} ターゲット=${KWIN_TARGET} 名前空間=${KWIN_NS}")

# ---------------------------------------------------------------------------
# ECM (KDEの共通CMakeモジュール)
# ---------------------------------------------------------------------------
find_package(ECM 6.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

# ---------------------------------------------------------------------------
# Qt6 / KF6 (KCMが使うQtとKDEフレームワークの依存)
# ---------------------------------------------------------------------------
# Qt6::QuickはKCMのUI生成で参照されるため要求する
find_package(Qt6 6.6 REQUIRED COMPONENTS
    Core
    Gui
    DBus
    Widgets
    Quick
)

# KCMが要求するKF6コンポーネントをまとめて要求する
find_package(KF6 6.0 REQUIRED COMPONENTS
    Config
    CoreAddons
    I18n
    KCMUtils
    WidgetsAddons
    ConfigWidgets
)

# ---------------------------------------------------------------------------
# KWin 本体 (フレーバに応じて KWin または KWinX11)
# ---------------------------------------------------------------------------
# KWinの開発ファイルを探索し、必要に応じて追加の探索先を利用する
# 探索先には[CMAKE_PREFIX_PATH]や[QT_ADDITIONAL_PACKAGES_PREFIX_PATH]も含まれる
find_package(${KWIN_PACKAGE} 6.4 REQUIRED)

# ---------------------------------------------------------------------------
# KWinエフェクトAPIの世代判定
# ---------------------------------------------------------------------------
# 6.5以降のWayland版KWinはエフェクトAPIが変更されている
# (core/region.hの追加、prePaintScreenのpresentTime引数削除など)
# ただしX11版KWin(KWinX11)は6.7でも従来のAPIのままで、パッケージ版数とAPI世代が一致しない
# そのため版数ではなく、実際に導入されているヘッダの有無で判定する
# 結果は[FACETS_KWIN_NEW_API]としてエフェクト本体へ伝える
set(FACETS_KWIN_NEW_API OFF)
if(TARGET ${KWIN_TARGET})
    get_target_property(_facets_kwin_incdirs ${KWIN_TARGET} INTERFACE_INCLUDE_DIRECTORIES)
    foreach(_dir IN LISTS _facets_kwin_incdirs)
        if(EXISTS "${_dir}/core/region.h")
            set(FACETS_KWIN_NEW_API ON)
        endif()
    endforeach()
endif()
message(STATUS "facets: KWin API世代=${FACETS_KWIN_NEW_API} (${KWIN_PACKAGE} 版数=${${KWIN_PACKAGE}_VERSION})")

# ---------------------------------------------------------------------------
# 出力先 (kcm側の保険と同じ場所にそろえる)
# ---------------------------------------------------------------------------
if(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()

# ---------------------------------------------------------------------------
# サブディレクトリ
# ---------------------------------------------------------------------------
# エフェクト本体
add_subdirectory(src/effect)

# 設定モジュールのビルド定義が存在する場合だけ追加する
# これによりkcmが無い状態でもエフェクト単体でビルドできる
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/kcm/CMakeLists.txt")
    add_subdirectory(src/kcm)
else()
    message(STATUS "facets: src/kcm/CMakeLists.txt が無いため、KCMをスキップします")
endif()

# 翻訳カタログ (po/<言語>/kwin_effect_facets.po を .mo へ変換し[KDE_INSTALL_LOCALEDIR]へ配置する)
# 英語は原文のため".po"を持たない (テンプレートは、po/kwin_effect_facets.pot)
# ki18n_install()はpoディレクトリをglobで探すため、パスに"[...]"を含むと何も見つけられない
# そのため、言語を明示してmsgfmtで変換する 言語を追加したらここへ追記すること
# 出力は言語ごとのディレクトリへ分ける (gettext_process_po_files()は言語を含まない同名の.gmoを作り衝突するため)
find_package(Gettext REQUIRED)
set(FACETS_TRANSLATIONS ja)
set(_facets_mo_files)
foreach(_lang IN LISTS FACETS_TRANSLATIONS)
    set(_po "${CMAKE_CURRENT_SOURCE_DIR}/po/${_lang}/kwin_effect_facets.po")
    set(_mo_dir "${CMAKE_CURRENT_BINARY_DIR}/locale/${_lang}/LC_MESSAGES")
    set(_mo "${_mo_dir}/kwin_effect_facets.mo")
    add_custom_command(OUTPUT "${_mo}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_mo_dir}"
        COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} --check -o "${_mo}" "${_po}"
        DEPENDS "${_po}"
        COMMENT "翻訳カタログを生成: ${_lang}"
        VERBATIM
    )
    list(APPEND _facets_mo_files "${_mo}")
    install(FILES "${_mo}" DESTINATION "${KDE_INSTALL_LOCALEDIR}/${_lang}/LC_MESSAGES")
endforeach()
add_custom_target(facets_translations ALL DEPENDS ${_facets_mo_files})
