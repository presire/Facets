/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

/**
 * @file Face.qml
 * @brief 3D空間内で1つのデスクトップを映す画面サイズの矩形モデル
 *
 * ScreenViewがキューブ・フリップ・デッキの各面として生成する
 * 100x100の単位矩形メッシュを[faceSize]の縦横比へ拡大し、[DefaultMaterial]の
 * [diffuseMap]に[DesktopView]をテクスチャとして貼り付けて描画する
 *
 * 入力 : [desktop] (表示する仮想デスクトップ)、[screen] (対象スクリーン)、
 *        [faceSize] (面の実サイズ)、[keepPanels] (パネルを固定するか)、
 *        [brightness] (面の明るさ 0.0-1.0)
 * 出力 : なし (描画のみ)
 * 依存 : KWinComponents (テクスチャ元の[DesktopView]経由でKWinのウィンドウモデルへ接続)
 */
import QtQuick
import QtQuick3D

// 画面と同じ大きさの平面を3D空間に置き、生きたデスクトップでテクスチャする
// 単位矩形メッシュは100x100で+Z方向を向く
Model {
    id: face

    required property QtObject desktop              // この面に表示する仮想デスクトップ
    required property QtObject screen               // この面が属するスクリーン
    required property size faceSize                 // 面の実サイズ (画面のピクセル寸法)
    property bool keepPanels: true                  // パネル・通知を3D面から外して固定表示するか
    property real brightness: 1.0                   // 面の明るさ係数 (1.0 = 最大輝度、0.0 = 黒)

    source: "#Rectangle"                            // QtQuick3D組み込みの単位矩形メッシュ
    scale: Qt.vector3d(faceSize.width / 100, faceSize.height / 100, 1)  // 100x100のメッシュを実サイズへ拡大

    materials: [
        DefaultMaterial {
            cullMode: Material.BackFaceCulling
            lighting: DefaultMaterial.NoLighting
            diffuseColor: Qt.rgba(face.brightness, face.brightness, face.brightness, 1)  // [brightness]で面の陰影を付ける
            diffuseMap: Texture {
                generateMipmaps: false
                sourceItem: DesktopView {           // [DesktopView]をそのままテクスチャとして描画する
                    desktop: face.desktop
                    screen: face.screen
                    keepPanels: face.keepPanels
                    width: face.faceSize.width
                    height: face.faceSize.height
                }
            }
        }
    ]
}
