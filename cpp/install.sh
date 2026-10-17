#!/usr/bin/env bash
#
# @file install.sh
# @brief facets (KWin 6)を現在のユーザ領域へビルドしてインストールする
#
# cpp/直下にbuild-waylandとbuild-x11を作り、Wayland版とX11版の両フレーバを$HOME/.localへ配置する
# [QT_PLUGIN_PATH]を設定するenvスクリプトも書き出し、再ログイン後にKWinからプラグインが見えるようにする
#
# 使い方 : install.sh [--uninstall | --remove-legacy]
# 終了コード : 0 = 正常終了、1 = ビルド失敗などの致命的なエラー
#

# 未定義変数の参照やコマンド失敗を即座に検出して中断する
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"                   # cppディレクトリの絶対パス
PLUGIN_ROOT="${PLUGIN_ROOT:-$HOME/.local/lib64/qt6/plugins}"                       # Qtプラグインの配置先 ([PLUGIN_ROOT]で上書き可)
ENV_DIR="$HOME/.config/plasma-workspace/env"                                       # startplasmaが読む環境変数ディレクトリ
ENV_FILE="$ENV_DIR/facets.sh"                                              # [QT_PLUGIN_PATH]を追加するenvスクリプト
QML_EFFECT_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/kwin/effects/facets"  # 同一idを持つ旧QML版エフェクト
LOCALE_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/locale"                           # 翻訳カタログの配置先 (KDEが探す[XDG_DATA_HOME]に合わせる)

#
# @brief 情報メッセージを標準出力へ表示する
# @param $* 表示するメッセージ
# @stdout ANSI色付きのメッセージ
# @return 0=常に成功
#
info() { printf '\033[1;34m==>\033[0m %s\n' "$*"; }

#
# @brief 警告メッセージを標準エラーへ表示する
# @param $* 表示するメッセージ
# @stdout なし (警告は標準エラーへ出力する)
# @return 0=常に成功
#
warn() { printf '\033[1;33m==> WARNING:\033[0m %s\n' "$*" >&2; }

#
# @brief エラーメッセージを標準エラーへ表示してスクリプトを終了する
# @param $* 表示するメッセージ
# @stdout なし (エラーは標準エラーへ出力する)
# @return なし (終了コード1で終了する)
#
die() { printf '\033[1;31m==> ERROR:\033[0m %s\n' "$*" >&2; exit 1; }

#
# @brief 使い方を表示する
# @stdout 使い方の説明
# @return 0=常に成功
#
usage() {
    cat <<'EOF'
使い方: install.sh [--uninstall | --remove-legacy]
  引数なし       ビルドして $HOME/.local へインストールする
  --remove-legacy  上に加え、旧QML版を削除せずバックアップへ退避する
  --uninstall    インストール済みファイルと設定を削除する
環境変数:
  PLUGIN_ROOT        Qtプラグイン配置先 (既定: $HOME/.local/lib64/qt6/plugins)
  KWIN_DEVEL_SYSROOT develパッケージのルート: 未設定ならシステムパスを使う
EOF
}

#
# @brief D-Bus経由で/Effectsのメソッドを呼び出す
# @param $1 呼び出すメソッド名 ([unloadEffect]など)
# @return 0=常に成功 (KWin未起動でも失敗させない)
# @note facets固定のidを対象にベストエフォートで呼び出す
#
effects_call() {
    # KWin未起動時も処理を続け、D-Busの応答と診断は破棄する
    if command -v qdbus6 >/dev/null 2>&1; then
        qdbus6 org.kde.KWin /Effects "org.kde.kwin.Effects.$1" facets >/dev/null 2>&1 || true
    else
        gdbus call --session --dest org.kde.KWin --object-path /Effects \
            --method "org.kde.kwin.Effects.$1" facets >/dev/null 2>&1 || true
    fi
}

#
# @brief facetsが現在ロード済みかを判定する
# @stdout なし (結果は終了コードで返す)
# @return 0=ロード済み、1=未ロード
#
effect_loaded() {
    local out=""
    if command -v qdbus6 >/dev/null 2>&1; then
        out="$(qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.isEffectLoaded facets 2>/dev/null || true)"
    else
        out="$(gdbus call --session --dest org.kde.KWin --object-path /Effects \
            --method org.kde.kwin.Effects.isEffectLoaded facets 2>/dev/null || true)"
    fi
    [[ "$out" == *true* ]]
}

#
# @brief 手動での有効化手順を表示する
# @stdout 有効化の手順
# @return 0=常に成功
#
print_manual_instructions() {
    cat <<'EOF'
有効化の手順:
  1. 一度ログアウトして再ログインする (WaylandとX11の両方で反映される)
  2. [システム設定] > [アニメーション] > [仮想デスクトップの切り替え]で"Facets"を選択する
EOF
}

#
# @brief 1つのフレーバをビルドしてインストールする
# @param $1 対象フレーバ ([wayland]または[x11])
# @global SCRIPT_DIR ビルド元ディレクトリ
# @global PLUGIN_ROOT Qtプラグインの配置先
# @global KWIN_DEVEL_SYSROOT develパッケージのルート (設定時のみ参照する)
# @global HOME ユーザ領域のインストール先の基準
# @stdout CMakeの設定・ビルド・インストールのログと進捗メッセージ
# @return 0=成功、1=設定・ビルド・インストールのいずれかに失敗
#
build_flavour() {
    local flavour="$1"
    local build_dir="$SCRIPT_DIR/build-$flavour"
    local -a args=(
        -DCMAKE_BUILD_TYPE=RelWithDebInfo
        -DCMAKE_INSTALL_PREFIX="$HOME/.local"
        -DKDE_INSTALL_QTPLUGINDIR="$PLUGIN_ROOT"
        -DKDE_INSTALL_LOCALEDIR="$LOCALE_DIR"
        -DKWIN_FLAVOR="$flavour"
    )
    # develパッケージを独自のsysrootに置く場合は探索パスを差し替える
    if [[ -n "${KWIN_DEVEL_SYSROOT:-}" ]]; then
        args+=(
            -DCMAKE_PREFIX_PATH="$KWIN_DEVEL_SYSROOT"
            -DQT_ADDITIONAL_PACKAGES_PREFIX_PATH="$KWIN_DEVEL_SYSROOT"
        )
    fi

    info "設定 ($flavour): $build_dir"
    if ! cmake -S "$SCRIPT_DIR" -B "$build_dir" "${args[@]}"; then
        warn "$flavour のCMake設定に失敗したためこのフレーバをスキップします (develパッケージ未導入の可能性)"
        warn "openSUSE の場合: sudo zypper install kwin6-devel kwin6-x11-devel qt6-quick-devel kf6-kcmutils-devel (詳細は、READMEを参照)"
        return 1
    fi
    # 条件式内で呼ばれるとset -eが効かないため失敗を明示的に伝播する
    if ! cmake --build "$build_dir" --parallel; then
        warn "$flavour のビルドに失敗しました"
        return 1
    fi
    if ! cmake --install "$build_dir"; then
        warn "$flavour のインストールに失敗しました"
        return 1
    fi
    return 0
}

#
# @brief [QT_PLUGIN_PATH]にプラグイン配置先を追加するenvスクリプトを冪等に書き出す
# @global ENV_DIR envスクリプトの配置ディレクトリ
# @global ENV_FILE 書き出すenvスクリプト
# @global PLUGIN_ROOT 追加するプラグイン配置先
# @stdout 進捗メッセージ
# @return 0=正常終了
# @note ファイル操作に失敗した場合はスクリプトを異常終了する
#
write_env_script() {
    mkdir -p "$ENV_DIR"
    # 既に同じ配置先が書かれていれば書き換えない
    if [[ -f "$ENV_FILE" ]] && grep -qF "$PLUGIN_ROOT" "$ENV_FILE"; then
        info "QT_PLUGIN_PATH 設定は既にある: $ENV_FILE"
        return
    fi
    info "QT_PLUGIN_PATH 設定を書き出す: $ENV_FILE"
    local quoted_root
    # 配置先に空白などが含まれても安全にexportできるよう引用する
    printf -v quoted_root '%q' "$PLUGIN_ROOT"
    # 既存の環境設定を残すためファイルへ追記する
    cat >>"$ENV_FILE" <<EOF
# facetsのQtプラグインをKWinから見えるようにする: 再ログイン後に有効
export QT_PLUGIN_PATH=${quoted_root}"\${QT_PLUGIN_PATH:+:\$QT_PLUGIN_PATH}"
EOF
}

#
# @brief 同一idを持つ旧QML版エフェクトを扱う
# @param $1 1なら旧QML版をバックアップへ退避する
# @global QML_EFFECT_DIR 旧QML版エフェクトのディレクトリ
# @global XDG_DATA_HOME バックアップ先の基準 (未設定なら[HOME]配下)
# @global HOME バックアップ先の基準 ([XDG_DATA_HOME]未設定時に参照する)
# @stdout 進捗メッセージ
# @stderr 警告メッセージ
# @return 0=正常終了
# @note ファイル操作に失敗した場合はスクリプトを異常終了する
# @note 退避せずに残すと同一idのためC++版と衝突する
#
handle_qml_effect() {
    [[ -d "$QML_EFFECT_DIR" ]] || return 0
    if [[ "$1" != "1" ]]; then
        warn "旧QML版エフェクトが残っています (同一idのため、C++版と衝突しています): $QML_EFFECT_DIR"
        warn "退避するには --remove-legacy を付けて再実行してください。"
        return 0
    fi
    local backup
    backup="${XDG_DATA_HOME:-$HOME/.local/share}/facets-qml-backup-$(date +%Y%m%d%H%M%S)"
    info "旧QML版エフェクトを退避する: $QML_EFFECT_DIR -> $backup"
    mv -- "$QML_EFFECT_DIR" "$backup"
}

#
# @brief 実行中KWinがプラグインを認識できる場合のみD-Busで再読み込みする
# @global PLUGIN_ROOT 実行中KWinが認識しているか検査する配置先
# @stdout 進捗メッセージと有効化の手順
# @stderr 警告メッセージ
# @return 0=常に成功
#
reload_or_instruct() {
    local pid=""
    pid="$(pidof -s kwin_x11 2>/dev/null || true)"
    [[ -n "$pid" ]] || pid="$(pidof -s kwin_wayland 2>/dev/null || true)"

    if [[ -z "$pid" ]]; then
        info "KWin は実行されていないため次回ログイン時に読み込まれます"
        return
    fi

    # 実行中プロセスの環境にプラグイン配置先が含まれるか確認する
    local environ=""
    if [[ -r "/proc/$pid/environ" ]]; then
        environ="$(tr '\0' '\n' <"/proc/$pid/environ" 2>/dev/null || true)"
    fi

    if [[ "$environ" != *"$PLUGIN_ROOT"* ]]; then
        warn "実行中の KWin (pid $pid) はまだ $PLUGIN_ROOT を認識していない"
        print_manual_instructions
        return
    fi

    # ロード済みのときだけunloadしてからloadし直す
    if effect_loaded; then
        info "facetsを再読み込みします"
        effects_call unloadEffect
    fi
    effects_call loadEffect
    info "facetsをD-Bus経由で読み込みました"
}

#
# @brief ビルドとインストールの本体を実行する
# @param $1 1なら旧QML版を退避する (既定0)
# @global PLUGIN_ROOT プラグインの配置先
# @stdout 進捗メッセージ
# @return 0=成功、1=両フレーバのビルド失敗またはファイル操作の失敗による異常終了
#
install_all() {
    local remove_legacy="${1:-0}"
    local ok=0
    local flavour
    for flavour in wayland x11; do
        if build_flavour "$flavour"; then
            ok=$((ok + 1))
        fi
    done
    ((ok > 0)) || die "wayland と x11の両方でビルドに失敗しました"

    write_env_script
    handle_qml_effect "$remove_legacy"
    reload_or_instruct
    info "インストール完了: $PLUGIN_ROOT"
}

#
# @brief インストール済みファイルと設定を削除する
# @global PLUGIN_ROOT 削除するプラグインの配置先
# @global ENV_FILE 削除するenvスクリプト
# @stdout 進捗メッセージ
# @return 0=正常終了
# @note ファイル操作に失敗した場合はスクリプトを異常終了する
#
uninstall_all() {
    # ロード済みなら実行中のKWinからアンロードする
    if effect_loaded; then
        effects_call unloadEffect
    fi

    local ns
    for ns in kwin kwin-x11; do
        rm -f "$PLUGIN_ROOT/$ns/effects/plugins/facets.so"
        rm -f "$PLUGIN_ROOT/$ns/effects/configs/kwin_facets_config.so"
    done

    # 設定画面の翻訳カタログ (全言語分)
    # 以前の版は[XDG_DATA_HOME]に関係なく$HOME/.local/share/localeへ配置していたため、そちらも消す
    rm -f "$LOCALE_DIR"/*/LC_MESSAGES/kwin_effect_facets.mo
    rm -f "$HOME"/.local/share/locale/*/LC_MESSAGES/kwin_effect_facets.mo

    rm -f "$ENV_FILE"

    if command -v kwriteconfig6 >/dev/null 2>&1; then
        kwriteconfig6 --file kwinrc --group Plugins --key facetsEnabled --delete || true
        # --keyを省略した--deleteはグループを消さないため、キーごとに削除する
        local key
        for key in Style Duration ZoomOut TiltAngle BackgroundDim Shading KeepPanels; do
            kwriteconfig6 --file kwinrc --group Effect-facets --key "$key" --delete || true
        done
    fi

    info "アンインストール完了: 再ログイン後に反映されます"
}

case "${1:-}" in
--uninstall | -u)
    # インストール済みファイルと設定を削除する
    uninstall_all
    ;;
--help | -h)
    # usageを表示して終了する
    usage
    ;;
"")
    # 引数なしはビルドしてインストールする (旧QML版は退避しない)
    install_all 0
    ;;
--remove-legacy)
    # ビルドに加え旧QML版をバックアップへ退避する
    install_all 1
    ;;
*)
    # 未知の引数はエラーにして指定できるオプションを示す
    die "不明な引数: $1 (--uninstall, --remove-legacy, --help)"
    ;;
esac
