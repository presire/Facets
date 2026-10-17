/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

/**
 * @file ScreenView.qml
 * @brief 1スクリーン分の3D切り替えシーンを描画するデリゲート
 *
 * main.qmlの[SceneEffect]の[delegate]として画面ごとに1インスタンス生成される
 * [effect.fromDesktop]から[effect.toDesktop]への遷移をキューブ・フリップ・デッキの
 * いずれかで描画し、カメラはz=0のWxH矩形が画面を覆う位置に置くため最初と最後のフレームが実デスクトップと一致する
 *
 * 入力 : [effect.fromDesktop]、[effect.toDesktop]、[effect.dx]、[effect.dy]、[effect.progress]、
 *        [effect.configuration] ([ZoomOut]・[TiltAngle]・[Shading]・[BackgroundDim]・[KeepPanels])
 * 出力 : [effect.viewReady]の呼び出しによる準備完了の通知、[effect.viewGone]の呼び出しによる破棄の通知
 * 依存 : KWinComponents ([SceneView]、[DesktopBackground]、[Workspace])、
 *        [Face.qml]、[DesktopView.qml]、[constants.js]
 */
import QtQuick
import QtQuick3D
import org.kde.kwin as KWinComponents

import "constants.js" as Constants

// この[Item]はスクリーンごとに生成され、デスクトップの遷移を小さな3Dシーンとして描画する
Item {
    id: root

    readonly property QtObject targetScreen: KWinComponents.SceneView.screen             // このビューが属するスクリーン (C++側SceneViewから取得)

    readonly property int style: effect.configuration.Style ?? Constants.Style.Cube         // [Style]設定
    readonly property real zoomOut: (effect.configuration.ZoomOut ?? 40) / 100              // [ZoomOut]は最大150なので0.0〜1.5の係数へ変換する
    readonly property real tiltAngle: effect.configuration.TiltAngle ?? 10                  // [TiltAngle]設定 (度)
    readonly property bool shading: effect.configuration.Shading ?? true                    // [Shading]設定
    readonly property real backgroundDim: (effect.configuration.BackgroundDim ?? 65) / 100  // [BackgroundDim]設定を0.0-1.0へ
    readonly property bool keepPanelsSetting: effect.configuration.KeepPanels ?? true       // [KeepPanels]設定
    // 全画面ウィンドウがパネルを覆い得る場合は固定を解除する
    // その場合はパネルを3D面に含め、デスクトップと一緒に動かす
    property bool keepPanels: keepPanelsSetting                                             // 実際にパネルを固定表示するかどうか

    // アニメーション状態 (共有時計はmain.qmlを参照)
    readonly property real progress: effect.progress                    // 進行度 (effectと共有)
    readonly property real bump: Math.sin(Math.PI * progress)           // 中間で最大になる山形の係数 (傾きに使う)
    readonly property bool horizontal: effect.dx !== 0                  // 横方向の切り替えかどうか
    readonly property int sign: horizontal ? effect.dx : effect.dy      // 移動方向の符号
    readonly property size faceSize: Qt.size(width, height)             // 面の実サイズ
    readonly property real depth: horizontal ? width : height           // 移動方向の奥行き

    readonly property real fieldOfView: 45                                                          // 垂直画角 (度)
    readonly property real fitDistance: 0.5 * height / Math.tan(0.5 * fieldOfView * Math.PI / 180)  // z=0の矩形が画面を覆うカメラ距離

    /**
     * @brief 進行方向に垂直な軸まわりの回転を作る
     * @param angle 回転角 (度)
     * @return 横移動ではY軸、縦移動ではX軸まわりのベクトル
     */
    function travelRotation(angle) {
        return horizontal ? Qt.vector3d(0, angle, 0) : Qt.vector3d(angle, 0, 0);
    }
    /**
     * @brief 進行方向に平行な軸まわりの回転を作る
     * @param angle 回転角 (度)
     * @return 横移動ではX軸、縦移動ではY軸まわりのベクトル
     */
    function tiltRotation(angle) {
        return horizontal ? Qt.vector3d(angle, 0, 0) : Qt.vector3d(0, angle, 0);
    }
    /**
     * @brief 視点から傾いた面の明るさを求める
     * @param angle 面の傾き (度)
     * @return 明るさ係数 (0.35-1.0)、[shading]が無効なら常に1
     */
    function facing(angle) {
        if (!shading) {
            return 1;
        }
        const c = Math.max(0, Math.cos(angle * Math.PI / 180));
        return 0.35 + 0.65 * c;
    }

    focus: true                                 // キー入力を受け取る
    Keys.onPressed: event => effect.finish()    // 任意のキー入力で残りのアニメーションをスキップする

    /**
     * @brief 指定デスクトップにフルスクリーンウィンドウがあるか判定する
     * @param desktop 対象のデスクトップ
     * @return このスクリーン上に非最小化・非非表示のフルスクリーンウィンドウがあればtrue
     * @note 全デスクトップ共通のウィンドウと現在のアクティビティのウィンドウも対象にする
     */
    function showsFullScreenWindow(desktop) {
        if (!desktop || !targetScreen) {
            return false;
        }
        return KWinComponents.Workspace.stackingOrder.some(window => window && window.fullScreen && !window.minimized && !window.hidden && window.output === targetScreen && (window.onAllDesktops || window.desktops.indexOf(desktop) >= 0) && (window.activities.length === 0 || window.activities.indexOf(KWinComponents.Workspace.currentActivity) >= 0));
    }

    /**
     * @brief [keepPanels]を再評価する
     * @note 全画面ウィンドウがパネルを覆い得る場合は固定をやめ、デスクトップと一緒に動かす
     */
    function updateKeepPanels() {
        keepPanels = keepPanelsSetting && !showsFullScreenWindow(effect.fromDesktop) && !showsFullScreenWindow(effect.toDesktop);
    }

    // 区間が切り替わるたびに[keepPanels]を再評価する
    Connections {
        target: effect
        /**
         * @brief 区間が切り替わったときに[keepPanels]を再評価する
         * @note [updateKeepPanels]を呼ぶ
         */
        function onGenerationChanged() {
            root.updateKeepPanels();
        }
    }
    Component.onCompleted: {
        updateKeepPanels();
        effect.viewReady();   // main.qmlへビューの準備完了を通知する
    }
    Component.onDestruction: effect.viewGone()

    // 3Dオブジェクトの周囲に見える背景: 切り替え先デスクトップの壁紙を暗くしたもの
    Rectangle {
        anchors.fill: parent
        color: "black"
    }
    // C++側のDesktopBackgroundが切り替え先デスクトップの壁紙を描画する
    KWinComponents.DesktopBackground {
        anchors.fill: parent
        activity: KWinComponents.Workspace.currentActivity
        desktop: effect.toDesktop
        outputName: root.targetScreen ? root.targetScreen.name : ""
        opacity: 1 - root.backgroundDim   // [BackgroundDim]の分だけ暗くする
    }

    // 3Dシーン本体 (最初と最後のフレームが実デスクトップと一致するようカメラを配置する)
    View3D {
        id: view
        anchors.fill: parent

        environment: SceneEnvironment {
            backgroundMode: SceneEnvironment.Transparent
            antialiasingMode: SceneEnvironment.MSAA
            antialiasingQuality: SceneEnvironment.High
        }

        // z=0の矩形が画面を覆う距離に置き、[bump]と[zoomOut]で中間だけカメラを引く
        PerspectiveCamera {
            fieldOfView: root.fieldOfView
            fieldOfViewOrientation: PerspectiveCamera.Vertical
            clipNear: 10
            clipFar: 100000
            position: Qt.vector3d(0, 0, root.fitDistance + root.bump * root.depth * (root.zoomOut + (root.style === Constants.Style.Flip ? 0.6 : 0)))
        }

        // --- Cube: 2つのデスクトップを箱の隣接面とし、中心まわりに回転させる
        Loader3D {
            active: root.style === Constants.Style.Cube
            sourceComponent: Node {
                position: Qt.vector3d(0, 0, -root.depth / 2)
                eulerRotation: root.tiltRotation(root.tiltAngle * root.bump)

                Node {
                    eulerRotation: root.travelRotation(-root.sign * 90 * root.progress)

                    Face {
                        desktop: effect.fromDesktop
                        screen: root.targetScreen
                        keepPanels: root.keepPanels
                        faceSize: root.faceSize
                        position: Qt.vector3d(0, 0, root.depth / 2)
                        brightness: root.facing(90 * root.progress)
                    }
                    Face {
                        desktop: effect.toDesktop
                        screen: root.targetScreen
                        keepPanels: root.keepPanels
                        faceSize: root.faceSize
                        position: root.horizontal ? Qt.vector3d(root.sign * root.width / 2, 0, 0) : Qt.vector3d(0, -root.sign * root.height / 2, 0)
                        eulerRotation: root.travelRotation(root.sign * 90)
                        brightness: root.facing(90 * (1 - root.progress))
                    }
                }
            }
        }

        // --- Flip: 2つのデスクトップをカードの表と裏とし、めくるように回転させる
        Loader3D {
            active: root.style === Constants.Style.Flip
            sourceComponent: Node {
                eulerRotation: root.tiltRotation(root.tiltAngle * root.bump)

                Node {
                    eulerRotation: root.travelRotation(-root.sign * 180 * root.progress)

                    Face {
                        desktop: effect.fromDesktop
                        screen: root.targetScreen
                        keepPanels: root.keepPanels
                        faceSize: root.faceSize
                        brightness: root.facing(180 * root.progress)
                    }
                    Face {
                        desktop: effect.toDesktop
                        screen: root.targetScreen
                        keepPanels: root.keepPanels
                        faceSize: root.faceSize
                        eulerRotation: root.travelRotation(180)
                        brightness: root.facing(180 * (1 - root.progress))
                    }
                }
            }
        }

        // --- Deck: 旧デスクトップは奥へ退き、新デスクトップは上に重なって手前へ回り込む
        Loader3D {
            active: root.style === Constants.Style.Deck
            sourceComponent: Node {
                eulerRotation: root.tiltRotation(root.tiltAngle * root.bump)

                Face {
                    desktop: effect.fromDesktop
                    screen: root.targetScreen
                    keepPanels: root.keepPanels
                    faceSize: root.faceSize
                    position: root.horizontal ? Qt.vector3d(-root.sign * root.width * 0.3 * root.progress, 0, -root.depth * 0.6 * root.progress) : Qt.vector3d(0, root.sign * root.height * 0.3 * root.progress, -root.depth * 0.6 * root.progress)
                    eulerRotation: root.travelRotation(root.sign * 20 * root.progress)
                    brightness: root.shading ? 1 - 0.6 * root.progress : 1
                }
                Face {
                    desktop: effect.toDesktop
                    screen: root.targetScreen
                    keepPanels: root.keepPanels
                    faceSize: root.faceSize
                    position: root.horizontal ? Qt.vector3d(root.sign * root.width * 1.1 * (1 - root.progress), 0, 0) : Qt.vector3d(0, -root.sign * root.height * 1.1 * (1 - root.progress), 0)
                    eulerRotation: root.travelRotation(root.sign * 30 * (1 - root.progress))
                }
            }
        }
    }

    // パネル・OSD・通知はアニメーションの手前に平坦かつ静止のまま重ねる
    DesktopView {
        anchors.fill: parent
        visible: root.keepPanels
        overlay: true
        desktop: effect.toDesktop
        screen: root.targetScreen
        keepPanels: root.keepPanels
    }

    // 任意のクリックで残りのアニメーションをスキップする
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        onPressed: effect.finish()
    }
}
