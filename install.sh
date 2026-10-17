#!/usr/bin/env bash
#
# @file install.sh
# @brief 現在のユーザに"Facets"仮想デスクトップ切り替えエフェクトをインストールする
#
# パッケージを~/.local/share/kwin/effects/配下へ配置する
# このディレクトリはkwin_waylandとkwin_x11の両方が走査するため、1回のインストールでWaylandとX11の両セッションを賄える
#
# 使い方 : install.sh [--uninstall]
# 終了コード : 0 = 正常終了、1 = packageが見つからない、またはファイル操作に失敗した
#

# コマンド失敗や未定義変数の参照を即座に検出して中断する
set -euo pipefail

ID=facets                                             # エフェクトの識別子
SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/package"   # packageディレクトリの絶対パス
DEST="${XDG_DATA_HOME:-$HOME/.local/share}/kwin/effects/$ID"  # エフェクトのインストール先

#
# @brief D-Bus経由で/Effectsのメソッドを呼び出す
# @param $1 呼び出すメソッド名 ([unloadEffect]、[loadEffect]、[isEffectLoaded]など)
# @global ID 対象エフェクトの識別子
# @stdout メソッドの応答 ([true]/[false]、gdbusでは[(true,)]など)
# @return 0=常に成功 (KWin未起動でも失敗させない)
# @note KWinが起動していない場合 (TTYからのインストールなど)は何も出力しない
#
dbus_effects() {
    # KWinが未起動でも処理を止めないようベストエフォートで呼ぶ
    if command -v qdbus6 >/dev/null 2>&1; then
        qdbus6 org.kde.KWin /Effects "org.kde.kwin.Effects.$1" "$ID" 2>/dev/null || true
    elif command -v gdbus >/dev/null 2>&1; then
        gdbus call --session --dest org.kde.KWin --object-path /Effects \
            --method "org.kde.kwin.Effects.$1" "$ID" 2>/dev/null || true
    fi
}

if [[ "${1:-}" == "--uninstall" ]]; then
    # 実行中のKWinからアンロードしてから本体を削除する
    dbus_effects unloadEffect >/dev/null
    # kwriteconfig6が無くても続行できるよう失敗を無視する
    kwriteconfig6 --file kwinrc --group Plugins --key "${ID}Enabled" --delete 2>/dev/null || true
    rm -rf -- "$DEST"
    echo "Removed $DEST"
    exit 0
fi

# packageに必要なファイルが揃っているかを検査する
if [[ ! -f "$SRC/metadata.json" || ! -f "$SRC/contents/ui/main.qml" ]]; then
    echo "error: package not found at $SRC" >&2
    exit 1
fi

WAS_LOADED=false  # インストール後に再ロードが必要かどうか
if dbus_effects isEffectLoaded | grep -q true; then
    WAS_LOADED=true
fi

# コピー失敗が稼働中のインストールを壊さないよう、まず隣のステージングへ複製する
mkdir -p "$(dirname "$DEST")"
STAGING="$(mktemp -d "$DEST.new.XXXXXX")"  # 一時的なステージングディレクトリ
trap 'rm -rf -- "$STAGING"' EXIT           # 途中終了時もステージングを残さない
cp -r "$SRC/." "$STAGING/"

if $WAS_LOADED; then
    dbus_effects unloadEffect >/dev/null
fi
rm -rf -- "$DEST"
mv -- "$STAGING" "$DEST"
trap - EXIT  # 移動に成功したため後片付けのtrapを解除する
echo "Installed to $DEST"

# 実行中のKWinへ上書きした場合は変更後のQMLを再読み込みさせる
if $WAS_LOADED; then
    if dbus_effects loadEffect | grep -q true; then
        echo "Reloaded the running effect."
    else
        echo "warning: could not reload the effect; re-select it in System Settings > Animations." >&2
    fi
else
    echo "Select it in System Settings > Animations > Virtual desktop switching > \"Facets\"."
fi
