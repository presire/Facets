/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

/**
 * @file DesktopView.qml
 * @brief 1つの仮想デスクトップを1つのスクリーン上に2Dで実時間描画するビュー
 *
 * ScreenViewから3D面のテクスチャ元として、またはパネル固定用のオーバーレイとして生成される
 * [overlay]がfalseのときは、[keepPanels]がtrueならパネル・OSD・通知を除き、falseならそれらも含めて描画する
 * [overlay]がtrueのときは、パネル・OSD・通知だけを3Dシーンの手前に平坦に描画する
 * [overlay]がtrueかつ[keepPanels]がfalseのときは描画対象が無いため何も描画しない
 *
 * 入力 : [desktop] (描画する仮想デスクトップ)、[screen] (対象スクリーン)、
 *        [overlay] (平坦オーバーレイとして描画するか)、[keepPanels] (パネルを固定するか)
 * 出力 : なし (描画のみ)
 * 依存 : KWinComponents ([Workspace]、[WindowFilterModel]、[WindowModel]、[WindowThumbnail])
 */
import QtQuick
import org.kde.kwin as KWinComponents

// [overlay]がfalseなら3D面側、trueならオーバーレイ側として、対象デスクトップのウィンドウを並べる
Item {
    id: desktopView

    required property QtObject desktop          // 描画対象の仮想デスクトップ
    required property QtObject screen           // 描画対象のスクリーン
    property bool overlay: false                // ピン留めウィンドウだけを描くオーバーレイかどうか
    property bool keepPanels: true              // ピン留めウィンドウをオーバーレイ側で描くかどうか

    /**
     * @brief 固定表示の対象となるウィンドウか判定する
     * @param window 判定するウィンドウ
     * @return パネル・通知・OSDならtrue
     */
    function isPinned(window) {
        return window.dock || window.notification || window.criticalNotification || window.onScreenDisplay;
    }

    clip: true

    // スクリーンで絞り込まない: 別出力のウィンドウでもこの画面へはみ出す場合がある
    // この画面外のウィンドウは[visible]で隠し、[clip]で切り取る
    Repeater {
        model: KWinComponents.WindowFilterModel {
            activity: KWinComponents.Workspace.currentActivity
            desktop: desktopView.desktop
            windowModel: KWinComponents.WindowModel {}
        }

        KWinComponents.WindowThumbnail {
            readonly property bool pinned: desktopView.keepPanels && desktopView.isPinned(model.window)                    // オーバーレイ対象のピン留めウィンドウか
            readonly property rect screenGeometry: desktopView.screen ? desktopView.screen.geometry : Qt.rect(0, 0, 0, 0)  // 対象スクリーンのジオメトリ
            // この画面と重なっているかどうか
            readonly property bool onThisScreen: model.window.x < screenGeometry.x + screenGeometry.width && model.window.x + model.window.width > screenGeometry.x && model.window.y < screenGeometry.y + screenGeometry.height && model.window.y + model.window.height > screenGeometry.y  // 対象スクリーンとウィンドウが重なっているか

            wId: model.window.internalId
            x: model.window.x - screenGeometry.x  // スクリーン原点を基準にした相対座標
            y: model.window.y - screenGeometry.y
            z: model.window.stackingOrder         // 重なり順をKWinのスタッキング順に一致させる
            width: model.window.width
            height: model.window.height
            // [overlay]側にはピン留めだけ、3D面側には非ピン留めだけを描く
            visible: onThisScreen && !model.window.minimized && !model.window.hidden && pinned === desktopView.overlay
        }
    }
}
