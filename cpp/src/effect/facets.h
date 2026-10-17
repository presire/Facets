/*
    facets — バーチャルデスクトップ切替時の 3D アニメーション

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "effect/effect.h"
#include "effect/effectwindow.h"

#if defined(FACETS_KWIN_NEW_API)
#include <core/region.h>
#endif

#include <QHash>
#include <QMatrix4x4>
#include <QPoint>
#include <QPointF>
#include <QRegion>
#include <QSet>
#include <QSize>
#include <QSizeF>
#include <QVector3D>

#include <array>
#include <chrono>
#include <memory>
#include <unordered_map>

namespace KWin
{

class GLFramebuffer;
class GLShader;
class GLTexture;
class LogicalOutput;
class Output;
class RenderView;
class VirtualDesktop;

/**
 * @typedef ScreenOutput
 * @brief エフェクトAPIが扱う出力の型
 *
 * 6.5以降のWayland版KWinは[LogicalOutput]、それ以外は[Output]を使う
 */
#if defined(FACETS_KWIN_NEW_API)
using ScreenOutput = LogicalOutput;
#else
using ScreenOutput = Output;
#endif

/**
 * @typedef DeviceRegion
 * @brief エフェクトAPIが扱う描画領域の型
 *
 * 6.5以降のWayland版KWinは[Region]、それ以外は[QRegion]を使う
 */
#if defined(FACETS_KWIN_NEW_API)
using DeviceRegion = Region;
#else
using DeviceRegion = QRegion;
#endif

/**
 * @class FacetsEffect
 * @brief デスクトップ切替を3Dトランジションで描くエフェクト
 *
 * 旧デスクトップと新デスクトップをそれぞれ別のオフスクリーンテクスチャへ描画し
 * 透視投影カメラと独自GLSLシェーダで立方体/カード/デッキとして合成する
 */
class FacetsEffect : public Effect
{
    Q_OBJECT

public:
    /**
     * @brief コンストラクタ
     *
     * EffectsHandlerの各シグナルを接続し、設定を読み込む
     */
    FacetsEffect();

    /**
     * @brief デストラクタ
     *
     * アニメーション中なら終了処理を行い、GLリソースを解放する
     */
    ~FacetsEffect() override;

    /**
     * @brief このエフェクトが実行環境で利用可能かを返す
     * @return OpenGL合成かつアニメーション有効ならtrue
     */
    static bool supported();

    /**
     * @brief 設定変更を各メンバへ読み込む
     * @param flags 再設定の種別 (ここでは未使用)
     */
    void reconfigure(ReconfigureFlags flags) override;

#if defined(FACETS_KWIN_NEW_API)
    /**
     * @brief 画面描画前にアニメーションの進捗を更新する
     * @param data 画面描画の前処理データ
     *
     * 6.5以降はpresentTimeが渡されないため[data.frame]の提示予定時刻を使う
     */
    void prePaintScreen(ScreenPrePaintData &data) override;
#else
    /**
     * @brief 画面描画前にアニメーションの進捗を更新する
     * @param data 画面描画の前処理データ
     * @param presentTime 現在の提示時刻
     */
    void prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime) override;
#endif

    /**
     * @brief 画面全体を合成して出力する
     * @param renderTarget 描画先
     * @param viewport ビューポート
     * @param mask 描画マスク
     * @param region 更新領域
     * @param screen 対象の出力
     */
    void paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const DeviceRegion &region, ScreenOutput *screen) override;

    /**
     * @brief 画面描画後に終了判定と再描画要求を行う
     */
    void postPaintScreen() override;

#if defined(FACETS_KWIN_NEW_API)
    /**
     * @brief 各ウィンドウの描画前処理を行う
     * @param view 対象の描画ビュー
     * @param w 対象ウィンドウ
     * @param data ウィンドウ描画の前処理データ
     */
    void prePaintWindow(RenderView *view, EffectWindow *w, WindowPrePaintData &data) override;
#else
    /**
     * @brief 各ウィンドウの描画前処理を行う
     * @param w 対象ウィンドウ
     * @param data ウィンドウ描画の前処理データ
     * @param presentTime 現在の提示時刻
     */
    void prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime) override;
#endif

#if defined(FACETS_KWIN_NEW_API)
    /**
     * @brief 現在のキャプチャパスに応じて1枚のウィンドウを描画する
     * @param renderTarget 描画先
     * @param viewport ビューポート
     * @param w 対象ウィンドウ
     * @param mask 描画マスク
     * @param region 更新領域
     * @param data ウィンドウ描画データ
     */
    void paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const DeviceRegion &region, WindowPaintData &data) override;
#else
    /**
     * @brief 現在のキャプチャパスに応じて1枚のウィンドウを描画する
     * @param renderTarget 描画先
     * @param viewport ビューポート
     * @param w 対象ウィンドウ
     * @param mask 描画マスク
     * @param region 更新領域
     * @param data ウィンドウ描画データ
     */
    void paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, QRegion region, WindowPaintData &data) override;
#endif

    /**
     * @brief アニメーション中かどうかを返す
     * @return 動作中またはジェスチャ追従中ならtrue
     */
    bool isActive() const override;

    /**
     * @brief エフェクトチェーン内の優先位置を返す
     * @return 他のフルスクリーン効果と競合しにくい固定値
     */
    int requestedEffectChainPosition() const override;

private Q_SLOTS:
    /**
     * @brief デスクトップ切替の確定をアニメーション開始へ変換する
     * @param oldDesktop 切替元デスクトップ
     * @param newDesktop 切替先デスクトップ
     * @param with 一緒に移動するウィンドウ
     */
    void desktopChanged(VirtualDesktop *oldDesktop, VirtualDesktop *newDesktop, EffectWindow *with);

    /**
     * @brief スワイプ中のデスクトップ切替を追従させる
     * @param currentDesktop 現在のデスクトップ
     * @param offset スワイプ量
     * @param with 一緒に移動するウィンドウ
     */
    void desktopChanging(VirtualDesktop *currentDesktop, QPointF offset, EffectWindow *with);

    /**
     * @brief スワイプが取り消されたときに元の進捗へ戻す
     */
    void desktopChangingCancelled();
#ifdef FACETS_HAVE_DESKTOP_MOVED
    /**
     * @brief デスクトップの並び替え通知 (進行中のアニメーションは崩さない)
     * @param desktop 対象デスクトップ
     * @param position 新しい位置
     */
    void desktopMoved(VirtualDesktop *desktop, int position);
#endif

    /**
     * @brief 切替中に追加されたウィンドウを対象へ振り分ける
     * @param w 追加されたウィンドウ
     */
    void windowAdded(EffectWindow *w);

    /**
     * @brief 削除されたウィンドウを各集合から取り除く
     * @param w 削除されたウィンドウ
     */
    void windowDeleted(EffectWindow *w);

    /**
     * @brief 出力追加時にGLリソースを作り直す
     * @param output 追加された出力
     */
    void outputAdded(ScreenOutput *output);

    /**
     * @brief 出力削除時に該当のGLリソースを破棄する
     * @param output 削除された出力
     */
    void outputRemoved(ScreenOutput *output);

private:
    /**
     * @enum Style
     * @brief トランジションの見た目
     */
    enum class Style {
        Cube, ///< 立方体の2面を回転させる
        Flip, ///< 1枚のカードを180度回転させる
        Deck  ///< 旧面を奥へ、新面を横から滑り込ませる
    };

    /**
     * @enum Pass
     * @brief 現在の描画パス
     *
     * オフスクリーンキャプチャ中にどの面へウィンドウを描くかを切り替える
     */
    enum class Pass {
        None,       ///< キャプチャ外 (通常描画)
        From,       ///< 旧デスクトップの面
        To,         ///< 新デスクトップの面
        Background, ///< 面の背後に敷く壁紙 (新デスクトップのdesktopウィンドウのみ)
        Overlay     ///< パネルなどの固定オーバーレイ
    };

    /**
     * @struct OutputState
     * @brief 出力ごとのオフスクリーンリソース
     */
    struct OutputState {
        std::unique_ptr<GLTexture> fromTexture;               ///< 旧デスクトップの色テクスチャ
        std::unique_ptr<GLFramebuffer> fromFramebuffer;       ///< 旧デスクトップのFBO
        std::unique_ptr<GLTexture> toTexture;                 ///< 新デスクトップの色テクスチャ
        std::unique_ptr<GLFramebuffer> toFramebuffer;         ///< 新デスクトップのFBO
        std::unique_ptr<GLTexture> overlayTexture;            ///< 固定オーバーレイの色テクスチャ
        std::unique_ptr<GLFramebuffer> overlayFramebuffer;    ///< 固定オーバーレイのFBO
        std::unique_ptr<GLTexture> backgroundTexture;         ///< 背景壁紙の色テクスチャ
        std::unique_ptr<GLFramebuffer> backgroundFramebuffer; ///< 背景壁紙のFBO
        QSize size;                                           ///< キャッシュ時のピクセルサイズ
        bool hdr = false;                                     ///< 作成時に浮動小数点形式を選んだか
        bool capturesValid = false;                           ///< 現在の切替で各テクスチャを撮影済みか
    };

    /**
     * @struct AnimationSpec
     * @brief アニメーション開始の指定
     */
    struct AnimationSpec {
        VirtualDesktop *from = nullptr; ///< 切替元デスクトップ
        VirtualDesktop *to = nullptr;   ///< 切替先デスクトップ
        EffectWindow *with = nullptr;   ///< 一緒に移動するウィンドウ
        qreal fromProgress = 0.0;       ///< 開始時の進捗 (ジェスチャ引継ぎ用)
        qreal toProgress = 1.0;         ///< 終了時の進捗 (キャンセル復帰用)
    };

    /**
     * @struct FaceState
     * @brief 1枚の面を描くための行列と明るさ
     */
    struct FaceState {
        QMatrix4x4 matrix;      ///< 面のモデル変換
        qreal brightness = 1.0; ///< 面の明るさ係数
    };

    /**
     * @struct QuadDraw
     * @brief クアッド描画のパラメータ
     */
    struct QuadDraw {
        QMatrix4x4 mvp;                   ///< 合成済みMVP行列
        std::array<QVector3D, 4> corners; ///< 左上/右上/右下/左下 (ローカル座標)
        bool mirrorU = false;             ///< u座標を反転するか
        bool mirrorV = false;             ///< v座標を反転するか
        GLTexture *texture = nullptr;     ///< 貼るテクスチャ
        float brightness = 1.0f;          ///< 明るさ
        float opacity = 1.0f;             ///< 不透明度
        float aaStrength = 0.0f;          ///< 縁アンチエイリアスの強さ
    };

    /**
     * @brief アニメーションを開始する
     * @param spec 開始条件 (切替元/先・進捗範囲)
     */
    void startAnimation(const AnimationSpec &spec);

    /**
     * @brief 切替前にウィンドウの可視参照を用意する
     */
    void prepareSwitching();

    /**
     * @brief 全出力の撮影済みテクスチャを無効にする
     */
    void invalidateCaptures();

    /**
     * @brief 切替中のウィンドウ構成の変化を反映して撮り直す
     */
    void refreshWindows();

    /**
     * @brief 切替後の後始末 (参照解放・フルスクリーン効果解除)
     */
    void finishedSwitching();

    /**
     * @brief 各面に含めるウィンドウ集合を再計算する
     */
    void collectWindows();

    /**
     * @brief 固定オーバーレイを出すかどうかを再判定する
     */
    void updateOverlayEnabled();

    /**
     * @brief ウィンドウを指定した面の集合へ振り分ける
     * @param w 振り分けるウィンドウ
     */
    void classifyWindow(EffectWindow *w);

    /**
     * @brief グリッド座標にあるデスクトップを返す
     * @param pos グリッド座標
     * @return 見つからなければnullptr
     */
    VirtualDesktop *desktopAtGrid(const QPoint &pos) const;

    /**
     * @brief 設定からスタイル列挙を求める
     * @return 設定値に対応するStyle
     */
    Style styleFromConfig() const;

    /**
     * @brief この切替で使う実効時間 (ミリ秒) を返す
     * @return 設定値、未設定なら速度係数から算出した時間
     */
    std::chrono::milliseconds animationDuration() const;

    /**
     * @brief 進捗に応じたイージング値を返す
     * @param t 線形な進捗 [0,1]
     * @return イージング済みの進捗
     */
    qreal easedProgress(qreal t) const;

    /**
     * @brief 出力のオフスクリーンリソースを必要サイズで用意する
     * @param state 対象出力の状態
     * @param screen 対象の出力
     * @return 用意できたらtrue
     */
    bool ensureFramebuffers(OutputState &state, ScreenOutput *screen);

    /**
     * @brief シーンを1パス分オフスクリーンへ描画する
     * @param state 対象出力の状態
     * @param pass 描画するパス
     */
    void capturePass(OutputState &state, Pass pass);

    /**
     * @brief 面とオーバーレイを合成して実レンダーターゲットへ出力する
     * @param state 対象出力の状態
     */
    void compositeScene(OutputState &state);

    /**
     * @brief 現在の出力サイズに対するカメラの透視投影行列を作る
     * @return 透視投影行列
     */
    QMatrix4x4 cameraProjection() const;

    /**
     * @brief 進捗から面の変換と明るさを求める
     * @param style トランジションの見た目
     * @param toFace 新デスクトップの面ならtrue、旧面ならfalse
     * @param progress 現在の進捗
     * @return 面の変換行列と明るさ
     */
    FaceState faceState(Style style, bool toFace, qreal progress) const;

    /**
     * @brief 1枚のクアッドを描画する
     * @param draw クアッドの描画パラメータ
     */
    void drawQuad(const QuadDraw &draw);

    /**
     * @brief シェーダのuniform位置を解決する
     */
    void resolveUniforms();

    int m_styleValue = 0;                                           ///< 設定の[Style]値 (0=Cube,1=Flip,2=Deck)
    std::chrono::milliseconds m_duration{0};                        ///< 設定の[Duration]ミリ秒 (0は自動)
    int m_zoomOut = 40;                                             ///< カメラの引き量%
    int m_tiltAngle = 10;                                           ///< 最大傾き度
    bool m_shading = true;                                          ///< 面を角度で暗くするか
    int m_backgroundDim = 65;                                       ///< 背景の減光%
    bool m_keepPanels = true;                                       ///< パネルを固定するか

    bool m_active = false;                                          ///< アニメーション中か
    bool m_finishRequested = false;                                 ///< 最終フレーム描画後に終了するか
    bool m_startTimeSet = false;                                    ///< 開始時刻を設定済みか
    std::chrono::milliseconds m_startTime{0};                       ///< アニメーション開始時刻
    qreal m_progress = 0.0;                                         ///< 現在の進捗 (時間駆動時はイージング済み)
    qreal m_progressFrom = 0.0;                                     ///< 開始時の進捗
    qreal m_targetProgress = 1.0;                                   ///< 目標の進捗

    VirtualDesktop *m_fromDesktop = nullptr;                        ///< 切替元デスクトップ
    VirtualDesktop *m_toDesktop = nullptr;                          ///< 切替先デスクトップ
    EffectWindow *m_movingWindow = nullptr;                         ///< 一緒に移動するウィンドウ
    int m_direction = 1;                                            ///< 進行方向 (+1/-1)
    bool m_vertical = false;                                        ///< 縦方向の切替か

    bool m_gestureActive = false;                                   ///< ジェスチャ追従中か
    qreal m_gestureProgress = 0.0;                                  ///< ジェスチャで決まった進捗

    QSet<EffectWindow *> m_fromWindows;                             ///< 旧面へ描くウィンドウ
    QSet<EffectWindow *> m_toWindows;                               ///< 新面へ描くウィンドウ
    QSet<EffectWindow *> m_overlayWindows;                          ///< オーバーレイへ描くウィンドウ
    QHash<EffectWindow *, EffectWindowVisibleRef> m_visibilityRefs; ///< 切替中の可視参照
    bool m_overlayEnabled = false;                                  ///< この切替でオーバーレイを出すか

    Pass m_pass = Pass::None;                                       ///< 現在のキャプチャパス

    std::unordered_map<ScreenOutput *, OutputState> m_outputs;     ///< 出力ごとのGLリソース

    // paintScreen呼び出し中だけ有効な現在の描画対象
    const RenderTarget *m_curRenderTarget = nullptr;                ///< 現在のレンダーターゲット
    const RenderViewport *m_curViewport = nullptr;                  ///< 現在のビューポート
    int m_curMask = 0;                                              ///< 現在の描画マスク
    DeviceRegion m_curRegion;                                       ///< 現在の描画領域
    ScreenOutput *m_curScreen = nullptr;                            ///< 現在の出力
    QSizeF m_curPixelSize;                                          ///< 現在の出力のピクセルサイズ

    std::unique_ptr<GLShader> m_shader;                             ///< 合成用シェーダ
    int m_mvpLocation = -1;                                         ///< u_mvpの位置
    int m_textureLocation = -1;                                     ///< u_textureの位置
    int m_brightnessLocation = -1;                                  ///< u_brightnessの位置
    int m_opacityLocation = -1;                                     ///< u_opacityの位置
    int m_aaLocation = -1;                                          ///< u_aaStrengthの位置
};

} // 名前空間の終わり
