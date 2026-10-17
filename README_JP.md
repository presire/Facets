# Facets

[English](README.md) | **日本語**

KDE Plasma 6向けの仮想デスクトップ切り替えアニメーションです。  
デスクトップを切り替える時に、横にスライドするのではなく、3Dの空間を通って移動します。  

## 概要

仮想デスクトップを切り替える時、KWinは通常、前のデスクトップを横に滑らせて次のデスクトップを出します。  
Facetsはその動きを短い3Dトランジションに置き換えます。  

KWinのデスクトップエフェクトとして動作するので、  
キーボードショートカット、ページャー、タッチパッドのジェスチャー等、デスクトップを切り替えるあらゆる場面で使えます。  

## 3つのスタイル

好みに合わせてトランジションを選択できます。  

- **Cube (キューブ)**  
  デスクトップが回転する3Dキューブの各面に並びます。  
  切り替えるとキューブが次の面へ回ります。  
- **Flip (フリップ)**  
  今のデスクトップがカードをめくるように裏返り、次のデスクトップが現れます。  
- **Deck (デッキ)**  
  前のデスクトップが奥へ沈み、新しいデスクトップが手前からスライドして入ってきます。  
  山札からカードを1枚引くような動きです。  

## 特徴

- 既定ではシステムのアニメーション速度に追従するので、Plasmaの他の動きと自然に馴染みます。  
- タッチパッドのジェスチャー、キーボードショートカット、デスクトップのページャーに対応します。  
- パネル、ドック、画面上の表示を固定して、その背後でデスクトップのみを動かすオプションがあります。  
- 3Dシーンの背後に表示する壁紙の暗さを調整できます。  
- カメラの引き具合(ズームアウト)と傾きを調整できます。  
- 視点から遠ざかる面を暗くする影の処理があります。  
- ディスプレイのリフレッシュレートに同期して、滑らかに動きます。  
  (C++版は、X11上の4K 60[Hz]のディスプレイで60[fps]を計測、標準のスライドエフェクトと同じ)  
- X11とWaylandの両方で動作します。  

## 2つのバージョン

Facetsのインストール方法は2つあります。  
どちらもCube・Flip・Deckを選択できますが、動きや描画方法には違いがあります。  

滑らかさや、ビルドに必要なものも異なります。  

| | C++版 | QML版 |
| --- | --- | --- |
| 品質 | 滑らか<br>ディスプレイのリフレッシュレートに同期 | 動作環境によります。<br>検証環境では約30[fps] |
| インストール | コンパイラを使用して、ソースコードからビルド | コピーのみ<br>コンパイラ不要 |

どちらのバージョンも同じエフェクトIDを使用するため、同時にインストールできるのは片方だけです。  
QML版がインストールされている状態でC++版をインストールしようとすると衝突します。  
C++版のインストーラはこれを警告し、古いQML版を退避させることもできます。(後述)  

## 動作要件

- KWinを含むKDE Plasma 6.x  
  OpenGL合成が有効であること。  
- QML版の場合:  
  コンパイラは不要ですが、Qt Quick 3DとKirigamiのランタイムモジュールが必要です。  
- C++版の場合、上記に加えて:  
  - CMake 3.16以降とC++20対応のコンパイラ  
  - Qt 6.6以降、KDE Frameworks (KF6) 6.0以降、および ECM (KDE Extra CMake Modules) の開発パッケージ  
  - 動作中のKWinに対応した開発パッケージ (6.4以降)  

Fedora 44の場合:  

```sh
sudo dnf install gcc-c++ cmake extra-cmake-modules qt6-qtbase-devel qt6-qtdeclarative-devel \
                 kf6-kconfig-devel kf6-kcoreaddons-devel kf6-ki18n-devel kf6-kcmutils-devel \
                 kf6-kwidgetsaddons-devel kf6-kconfigwidgets-devel kf6-kwindowsystem-devel \
                 kdecoration-devel libepoxy-devel libdrm-devel wayland-devel kwin-devel kwin-x11-devel
```

openSUSE Leap 16 / SLEでは、次のパッケージをインストールします。  

```sh
sudo zypper install gcc-c++ cmake kwin6-devel kwin6-x11-devel qt6-quick-devel \
                    kf6-kcmutils-devel kf6-kconfigwidgets-devel kf6-ki18n-devel kf6-kwindowsystem-devel \
                    kf6-kcoreaddons-devel kf6-kconfig-devel kf6-kwidgetsaddons-devel
```

`kwin6-x11-devel`はX11セッション用、`kwin6-devel`はWaylandセッション用です。  
インストーラはビルドできる方だけをビルドし、できない方は飛ばします。  

Debian 14 Forkyの場合:  

```sh
sudo apt install build-essential cmake extra-cmake-modules qt6-base-dev qt6-declarative-dev \
                 libkf6config-dev libkf6coreaddons-dev libkf6i18n-dev libkf6kcmutils-dev \
                 libkf6widgetsaddons-dev libkf6configwidgets-dev libkf6windowsystem-dev \
                 libkdecorations3-dev libepoxy-dev libdrm-dev libwayland-dev kwin-dev kwin-x11-dev
```

> 注意:  
> コンパイル済みのKWinエフェクトは、動作中のKWinと同じバージョンに対してビルドする必要があります。  
> KWinが変わるPlasmaアップデートの後は、C++版をビルドし直して再インストールしてください。  

## インストール

### QML版

このリポジトリをダウンロードまたはクローンし、プロジェクトのディレクトリで次を実行します:  

```sh
./install.sh
```

エフェクトがホームディレクトリ内 (既定では、`~/.local/share/kwin/effects/facets/`) にコピーされます。  
root権限は不要です。  

### C++版 (推奨)

このリポジトリをダウンロードまたはクローンし、プロジェクトのディレクトリで次を実行します:  

```sh
./cpp/install.sh
```

Wayland版とX11版の2つをビルドしてインストールします。  
配置先は、`~/.local/lib64/qt6/plugins/`です。(root権限は不要)  

開発パッケージが無い等の理由で片方だけしかビルドできなくても、インストールはビルド成功したもので完了します。  
両方が失敗した時だけスクリプトが停止します。  

また、次回ログイン後にKWinがプラグインを見つけられるようにする起動スクリプト`~/.config/plasma-workspace/env/facets.sh`も作成します。  
`~/.local/lib64`はQtが標準で探す場所に含まれないため、この起動スクリプトが環境変数`QT_PLUGIN_PATH`に`~/.local/lib64/qt6/plugins`を追加します。  
再ログインするまでは、KWinはエフェクトを見つけられません。  

オプション:  

```sh
./cpp/install.sh --remove-legacy   # 古いQML版も退避する
./cpp/install.sh --uninstall       # インストール済みファイルと設定を削除する
./cpp/install.sh --help            # 使い方を表示する
```

> `--remove-legacy`について:  
> 古いQML版がまだインストールされている場合、C++版と同じエフェクトIDを使うため衝突します。  
> 通常のインストールでは警告が出るだけです。  
> 
> `--remove-legacy`を付けて実行すると、古いQML版を削除せず、  
> ユーザのデータディレクトリ内のバックアップ (通常は、`~/.local/share/facets-qml-backup-<日時>`) へ移動します。  
> 元に戻す時のために残しておけます。  

### 再ログイン

C++版の場合は、インストール後に再ログインしてください。  
再ログイン時にプラグインの探索パスが有効になります。  

再起動は必要ありません。  

QML版では再ログインする必要はありません。  
既に有効になっていれば、インストーラがエフェクトを再読み込みします。  

そうでない場合は、システム設定でFacetsを選んでください。  

## 有効にする

1. システム設定を開きます。
2. **[ワークスペースの動作] > [デスクトップエフェクト]**へ進みます。  
   (Plasmaのバージョンによってはこのページは**アニメーション**という名前です)  
3. **[仮想デスクトップの切り替え]**の項目で**[Facets]**を選択します。  

同時に有効にできるアニメーションは1つだけです。  
Facetsを選ぶと、それまで選んでいたアニメーションが外れ、別のアニメーションを選ぶとFacetsがオフになります。  

## 設定を変更する

同じページで、Facetsの横にある設定ボタン (歯車アイコン) をクリックします。  

以下のラベルはC++版のものです。  
QML版でラベルが異なる場合はその旨を記しています。  

次の項目を変更できます。  

- **Style**  
  使用するトランジション: Cube、Flip、Deck
  既定: Cube  
- **Duration**  
  アニメーションの長さ (ミリ秒)  
  `0`は「自動」 (デフォルト) で、全体のアニメーション速度に追従します。  
  範囲は、0から3000  
- **Zoom out**  
  アニメーションの途中でカメラが引く量 (パーセント)  
  大きいほど3Dシーンが広く見えます。  
  範囲は、0から150  
  既定: 40  
- **Tilt angle**  
  3Dシーンの最大の傾き (度)  
  QML版では、この設定は**Tilt**という名前です。  
  範囲は、0から30
  既定: 10
- **Background dimming**  
  3Dシーンの背後に表示する壁紙の暗さ (パーセント)  
  範囲は、0から100  
  既定: 65  
- **Shade faces turned away from the viewer**  
  視点から遠ざかるデスクトップの面を暗くして、奥行きを出します。  
  QML版では、この設定は**Darken faces turning away**という名前です。  
  既定: オン
- **Keep panels and on-screen displays fixed**  
  パネル、ドック、画面上の表示を、デスクトップと一緒に動かさずその場に固定します。  
  QML版では、この設定は**Keep panels and notifications in place**という名前です。  
  既定: オン

## アンインストール

QML版の場合、プロジェクトのディレクトリで:  

```sh
./install.sh --uninstall
```

C++版の場合:

```sh
./cpp/install.sh --uninstall
```

どちらもインストール済みのエフェクトファイルを削除します。  

QML版のアンインストーラは、`~/.local/share/kwin/effects/facets/`を削除し、有効状態の消去も試みます。  
C++版のアンインストーラは、`~/.local/lib64/qt6/plugins/`配下のファイルと起動スクリプト`~/.config/plasma-workspace/env/facets.sh`を削除し、  
KDEの設定ツールが利用できる場合は有効状態とエフェクトの設定も消去します。  

C++版をアンインストールした場合は、1度ログアウトしてログインし直してください。  
QML版では再ログインする必要はありません。  

## 困った時 / FAQ

**エフェクトが一覧に出てこない**  
C++版をインストールした場合は、その後に1度ログアウトしてログインし直したか確認してください。  
C++版はプラグインのパスを設定する起動スクリプトに頼っており、それが効くのはログイン時です。  

もし、表示されない場合は、  
起動スクリプト`~/.config/plasma-workspace/env/facets.sh`が存在し、Plasmaセッションから読み込まれているかを確認してください。  

**以前は動いていたのに、Plasmaのアップデートで動かなくなった**  
コンパイル済みのKWinエフェクトは、動作中のKWinのバージョンと一致している必要があります。  
Plasmaのアップデート後は、C++版のインストールを再度実行して、新しいバージョン向けにビルドし直してください。  

**アニメーションが速すぎる、または遅すぎる**  
設定を開いて**Duration**を変更してください。  
自動にすると全体のアニメーション速度に追従し、ミリ秒で数値を入れると自分で長さを決められます。  

**有効にできるアニメーションが1つだけ**  
仕様です。  
KWinでは仮想デスクトップ切り替えのアニメーションは同時に1つのみであるため、Facetsを有効にすると前のものが外れ、逆も同じです。  

**C++版とQML版を両方残してもよい？**  
同じエフェクトIDを使って衝突するため、両方を残さないでください。  
インストーラは警告するだけで処理を続けます。  
もう片方を先にアンインストールするか、C++版のインストール時に`--remove-legacy`を付けて古いQML版を退避させてください。  

**タッチパッドのジェスチャーで切り替わらない**
Facetsは切り替えの見た目のみを変更します。(ジェスチャー自体はPlasmaが担当)  
システム設定のワークスペースの動作にあるタッチパッドのジェスチャー設定を確認してください。  

## ライセンス

Facetsはフリーソフトウェアで、**GPL-2.0-or-later**ライセンスで公開されています。  
ライセンスの表示は各ソースファイルを参照してください。  
