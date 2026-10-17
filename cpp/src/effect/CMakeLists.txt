# facetsエフェクト本体
#
# 親CMakeListsが[KWIN_NS]/[KWIN_TARGET]/[KWIN_FLAVOR]を設定している前提
# ツリー外ビルドなのでkwin_add_builtin_effectではなくkcoreaddons_add_pluginを使う

# GLSLは.qrcリソースとしてバイナリへ埋め込む(AUTORCCで生成)
set(facets_SRCS
    main.cpp
    facets.cpp
    facetsrender.cpp
    shaders.qrc
)

# 共有のKConfigXT定義からKWin::FacetsConfigを生成する
kconfig_add_kcfg_files(facets_SRCS
    ../facetsconfig.kcfgc
)

# エフェクト本体をプラグインとして追加し[KWIN_NS]配下へインストールする
kcoreaddons_add_plugin(facets
    SOURCES ${facets_SRCS}
    INSTALL_NAMESPACE "${KWIN_NS}/effects/plugins"
)

# [Q_OBJECT]/[Q_PLUGIN_METADATA]とmain.mocのため[AUTOMOC]を有効にする
# 埋め込みシェーダ(.qrc)をコンパイルするため[AUTORCC]も有効にする
set_target_properties(facets PROPERTIES
    AUTOMOC ON
    AUTORCC ON
)

# 生成されたfacetsconfig.hを取り込めるようにする
target_include_directories(facets PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
)

# KWin本体とKConfig GUIをリンクする
target_link_libraries(facets
    ${KWIN_TARGET}
    KF6::ConfigGui
)

# desktopMovedシグナルはWayland版KWinにのみ存在する
# X11版でもコンパイルできるよう[KWIN_FLAVOR]で分岐する
if(KWIN_FLAVOR STREQUAL "wayland")
    target_compile_definitions(facets PRIVATE FACETS_HAVE_DESKTOP_MOVED)
endif()

# エフェクトAPIの世代を渡す
# 親CMakeListsが実際のヘッダで判定した結果[FACETS_KWIN_NEW_API]を使う
# (版数だけではX11版の6.7が旧APIであることを判別できないため)
if(FACETS_KWIN_NEW_API)
    target_compile_definitions(facets PRIVATE FACETS_KWIN_NEW_API)
endif()

# 参考として導入パッケージの版数もマクロで公開する
if(DEFINED ${KWIN_PACKAGE}_VERSION_MAJOR)
    target_compile_definitions(facets PRIVATE
        FACETS_KWIN_VERSION_MAJOR=${${KWIN_PACKAGE}_VERSION_MAJOR}
        FACETS_KWIN_VERSION_MINOR=${${KWIN_PACKAGE}_VERSION_MINOR}
    )
endif()
