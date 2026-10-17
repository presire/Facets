# facets設定モジュール (KCM)
#
# KWinツリー外でビルドするためkwin_add_effect_config()は使えない
# 代わりにkcoreaddons_add_plugin()で同じ配置先へ出力する
# プラグインidは必ず"kwin_facets_config"にすること
# エフェクトのmetadata.jsonの[X-KDE-ConfigModule]と一致させる必要がある

# kcoreaddons_add_pluginは[CMAKE_LIBRARY_OUTPUT_DIRECTORY]配下へ出力する
# 親が未設定だと"/kwin/..."という書込不能な絶対パスになるため保険を張る
if(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()

# 親が[KWIN_NS]を設定しない場合の保険[KWIN_FLAVOR]から導出する
# 未設定のままだと配置先が"/effects/configs"になるため
if(NOT KWIN_NS)
    if(KWIN_FLAVOR STREQUAL "x11")
        set(KWIN_NS "kwin-x11")
    else()
        set(KWIN_NS "kwin")
    endif()
endif()

set(kwin_facets_config_SRCS
    facets_config.cpp
)

ki18n_wrap_ui(kwin_facets_config_SRCS
    facets_config.ui
)

# 共有のKConfigXT定義からKWin::FacetsConfigを生成する
kconfig_add_kcfg_files(kwin_facets_config_SRCS
    ../facetsconfig.kcfgc
)

kcoreaddons_add_plugin(kwin_facets_config
    SOURCES ${kwin_facets_config_SRCS}
    INSTALL_NAMESPACE "${KWIN_NS}/effects/configs"
)

# 翻訳ドメインは独自に持つ (KWin本体のカタログには本エフェクトの文字列が無いため)
# 両フレーバで共通とし、カタログはcpp/po/<言語>/kwin_effect_facets.poから生成する
target_compile_definitions(kwin_facets_config PRIVATE
    -DTRANSLATION_DOMAIN="kwin_effect_facets"
)

target_include_directories(kwin_facets_config PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(kwin_facets_config
    KF6::KCMUtils
    KF6::CoreAddons
    KF6::ConfigCore
    KF6::ConfigGui
    KF6::I18n
    Qt6::DBus
    Qt6::Widgets
)
