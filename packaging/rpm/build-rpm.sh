#!/usr/bin/env bash
#
# @file build-rpm.sh
# @brief Facets (C++版KWinエフェクト) のRPMをビルドするドライバスクリプト
#
# リポジトリ内のファイルからSource0のtarballを生成し、同ディレクトリの
# facets.specをrpmbuild -baでビルドして、生成されたRPMを<repo>/dist/へ
# 集約する。ディストリビューションのコンテナ内でリポジトリルートをカレントにして実行する想定:
#
#   packaging/rpm/build-rpm.sh .leap16
#
# 使い方: build-rpm.sh [dist-tag]
#   dist-tagは%{dist}に設定する文字列 (例: .leap16.0, .tumbleweed, .fc44)。
#   省略した場合はディストリビューション既定の%{dist}がそのまま使われる
#   (Fedoraでは既に.fc44が定義されているため、その既定値が優先される)。
# 終了コード: 0 = 正常終了、1 = 引数不正、必須ファイル欠如、ビルド失敗
#

set -euo pipefail

# --- パス解決 --------------------------------------------------------------
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)" # packaging/rpm
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../.." && pwd)" # リポジトリルート
SPEC="$SCRIPT_DIR/facets.spec"
CMAKE_FILE="$REPO_ROOT/cpp/CMakeLists.txt"
BUILD_DIR="$REPO_ROOT/rpmbuild"
DIST_DIR="$REPO_ROOT/dist"

# --- 引数 ------------------------------------------------------------------
DIST_TAG="${1:-}"

# --- バージョンはcpp/CMakeLists.txtのproject()を唯一の正とする ----------
if [[ ! -f "$CMAKE_FILE" ]]; then
    echo "error: cpp/CMakeLists.txt が見つかりません: $CMAKE_FILE" >&2
    exit 1
fi
VERSION="$(sed -n 's/^project(facets VERSION \([0-9][0-9.]*\).*/\1/p' "$CMAKE_FILE" | head -n1)"
if [[ -z "$VERSION" ]]; then
    echo "error: cpp/CMakeLists.txt から project(facets VERSION ...) を取得できません" >&2
    exit 1
fi
if [[ ! -f "$SPEC" ]]; then
    echo "error: spec ファイルが見つかりません: $SPEC" >&2
    exit 1
fi

# --- rpmbuild用ディレクトリを準備 ----------------------------------------
mkdir -p "$BUILD_DIR"/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}

# --- Source0のtarballを生成 --------------------------------------------
# 一時ステージングへ必要ファイルだけを複製し、トップディレクトリが
# facets-<version>/になるようtarで圧縮する。
TARBALL="$BUILD_DIR/SOURCES/facets-$VERSION.tar.gz"
STAGE="$(mktemp -d)"
trap 'rm -rf -- "$STAGE"' EXIT
STAGE_ROOT="$STAGE/facets-$VERSION"
mkdir -p "$STAGE_ROOT"

tar -C "$REPO_ROOT" \
    --exclude='./.git' --exclude='./.git/*' \
    --exclude='./.github' --exclude='./.github/*' \
    --exclude='./.omo' \
    --exclude='./cpp/build' --exclude='./cpp/build-*' \
    --exclude='./build' --exclude='./build-*' \
    --exclude='./stage' \
    --exclude='./rpmbuild' --exclude='./dist' \
    -cf - . | tar -C "$STAGE_ROOT" -xf -

tar -C "$STAGE" -czf "$TARBALL" "facets-$VERSION"
echo "info: Source0 tarball を生成しました: $TARBALL"

# --- rpmbuild実行 ---------------------------------------------------------
# dist-tagが指定されたときだけ%{dist}を上書きする。未指定ならディストリビューション既定。
RPMBUILD_ARGS=(--define "_topdir $BUILD_DIR")
if [[ -n "$DIST_TAG" ]]; then
    RPMBUILD_ARGS+=(--define "dist $DIST_TAG")
fi

rpmbuild -ba "${RPMBUILD_ARGS[@]}" "$SPEC"

# --- 生成物をdist/へ集約 -------------------------------------------------
mkdir -p "$DIST_DIR"
# .rpmのみをコピーする (直下に出力先が無くてもfindは空振りで成功する)
find "$BUILD_DIR/RPMS" -type f -name '*.rpm' -exec cp -f {} "$DIST_DIR/" \;

echo "info: 生成した RPM:"
find "$DIST_DIR" -maxdepth 1 -type f -name '*.rpm' -printf '  %p\n' | sort
