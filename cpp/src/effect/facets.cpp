/*
    facets — バーチャルデスクトップ切替時の 3D アニメーション

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "facets.h"
#include "facetsconfig.h"

#include <QPoint>
#include <QPointF>
#include <QSizeF>
#include <QVector3D>

#include <core/output.h>
#if defined(FACETS_KWIN_NEW_API)
#include <core/renderbackend.h>
#endif
#include <effect/effecthandler.h>
#include <effect/effectwindow.h>
#include <opengl/glutils.h>
#include <virtualdesktops.h>

#include <algorithm>
#include <cmath>

namespace KWin
{

/** @brief [Duration]未設定のときにanimationTimeFactorへ掛ける基準時間(ミリ秒) */
static constexpr int kBaseDurationMs = 450;

/**
 * @brief コンストラクタ
 */
FacetsEffect::FacetsEffect()
    : Effect()
{
    // 設定シングルトンをハンドラのconfigに紐づける
    FacetsConfig::instance(effects->config());

    connect(effects, &EffectsHandler::desktopChanged, this, &FacetsEffect::desktopChanged);
    connect(effects, &EffectsHandler::desktopChanging, this, &FacetsEffect::desktopChanging);
    connect(effects, &EffectsHandler::desktopChangingCancelled, this, &FacetsEffect::desktopChangingCancelled);
#ifdef FACETS_HAVE_DESKTOP_MOVED
    connect(effects, &EffectsHandler::desktopMoved, this, &FacetsEffect::desktopMoved);
#endif
    connect(effects, &EffectsHandler::windowAdded, this, &FacetsEffect::windowAdded);
    connect(effects, &EffectsHandler::windowDeleted, this, &FacetsEffect::windowDeleted);
    connect(effects, &EffectsHandler::screenAdded, this, &FacetsEffect::outputAdded);
    connect(effects, &EffectsHandler::screenRemoved, this, &FacetsEffect::outputRemoved);
    // ロックされたら撮影済みの内容を描かないよう直ちに終了する
    connect(effects, &EffectsHandler::screenLockingChanged, this, [this](bool locked) {
        if (locked && m_active) {
            finishedSwitching();
        }
    });

    reconfigure(ReconfigureAll);
}

/**
 * @brief デストラクタ
 */
FacetsEffect::~FacetsEffect()
{
    if (m_active) {
        finishedSwitching();
    }
    // GLリソースの解放前にコンテキストをcurrentにする
    if (effects) {
        effects->makeOpenGLContextCurrent();
    }
    m_outputs.clear();
    m_shader.reset();
}

/**
 * @brief このエフェクトが実行環境で利用可能かを返す
 * @return 利用可能ならtrue
 */
bool FacetsEffect::supported()
{
    // OpenGL合成かつアニメーション有効なときのみ動く
    return effects->isOpenGLCompositing() && effects->animationsSupported();
}

/**
 * @brief 設定変更を各メンバへ読み込む
 * @param flags 再設定の種別 (ここでは未使用)
 */
void FacetsEffect::reconfigure(ReconfigureFlags flags)
{
    Q_UNUSED(flags)
    FacetsConfig::self()->read();
    m_styleValue = FacetsConfig::style();
    m_duration = std::chrono::milliseconds(FacetsConfig::duration());
    m_zoomOut = FacetsConfig::zoomOut();
    m_tiltAngle = FacetsConfig::tiltAngle();
    m_shading = FacetsConfig::shading();
    m_backgroundDim = FacetsConfig::backgroundDim();
    m_keepPanels = FacetsConfig::keepPanels();
}

/**
 * @brief この切替で使う実効時間 (ミリ秒) を返す
 * @return 設定値、未設定なら速度係数から算出した時間
 */
std::chrono::milliseconds FacetsEffect::animationDuration() const
{
    if (m_duration.count() > 0) {
        return m_duration;
    }
    // 未設定なら全体のアニメーション速度係数から自動決定する
    const long scaled = std::lround(effects->animationTimeFactor() * double(kBaseDurationMs));
    return std::chrono::milliseconds(std::max(0L, scaled));
}

/**
 * @brief 設定からスタイル列挙を求める
 * @return 設定値に対応するStyle
 */
FacetsEffect::Style FacetsEffect::styleFromConfig() const
{
    switch (m_styleValue) {
    case 1:
        return Style::Flip;
    case 2:
        return Style::Deck;
    default:
        return Style::Cube;
    }
}

/**
 * @brief 進捗に応じたイージング値を返す
 * @param t 線形な進捗 [0,1]
 * @return イージング済みの進捗
 */
qreal FacetsEffect::easedProgress(qreal t) const
{
    t = std::clamp(t, 0.0, 1.0);
    if (styleFromConfig() == Style::Deck) {
        // 新面のスライドインは減速カーブ(OutCubic)
        const qreal u = 1.0 - t;
        return 1.0 - u * u * u;
    }
    // それ以外はInOutCubic
    if (t < 0.5) {
        return 4.0 * t * t * t;
    }
    return 1.0 - std::pow(-2.0 * t + 2.0, 3.0) / 2.0;
}

#if defined(FACETS_KWIN_NEW_API)
/**
 * @brief 画面描画前にアニメーションの進捗を更新する
 * @param data 画面描画の前処理データ
 *
 * 6.5以降はpresentTimeが渡されないため[data.frame]の提示予定時刻を使う
 */
void FacetsEffect::prePaintScreen(ScreenPrePaintData &data)
{
    const std::chrono::milliseconds presentTime = data.frame
        ? std::chrono::duration_cast<std::chrono::milliseconds>(data.frame->targetPageflipTime().time_since_epoch())
        : std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch());
#else
/**
 * @brief 画面描画前にアニメーションの進捗を更新する
 * @param data 画面描画の前処理データ
 * @param presentTime 現在の提示時刻
 */
void FacetsEffect::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
{
#endif
    if (m_active) {
        if (m_gestureActive) {
            // ジェスチャ中は指の位置で進捗が決まるので時間では進めない
            m_progress = std::clamp(m_gestureProgress, 0.0, 1.0);
        } else {
            if (!m_startTimeSet) {
                m_startTime = presentTime;
                m_startTimeSet = true;
            }
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(presentTime - m_startTime);
            const qreal durationMs = std::max<qreal>(1.0, qreal(animationDuration().count()));
            const qreal raw = std::clamp(qreal(elapsed.count()) / durationMs, 0.0, 1.0);
            // 時間軸にだけイージングを掛け、開始/目標の進捗の間を補間する
            m_progress = m_progressFrom + (m_targetProgress - m_progressFrom) * easedProgress(raw);
            if (raw >= 1.0) {
                m_finishRequested = true;
            }
        }
        // 6.5以降は背景を最初に消去するフラグが無くなったため変換マークのみ付ける
#if defined(FACETS_KWIN_NEW_API)
        data.mask |= PAINT_SCREEN_TRANSFORMED;
#else
        data.mask |= PAINT_SCREEN_TRANSFORMED | PAINT_SCREEN_BACKGROUND_FIRST;
#endif
    }
#if defined(FACETS_KWIN_NEW_API)
    effects->prePaintScreen(data);
#else
    effects->prePaintScreen(data, presentTime);
#endif
}

/**
 * @brief 全出力の描画後に終了判定と再描画要求を行う
 */
void FacetsEffect::postPaintScreen()
{
    if (m_active) {
        if (m_finishRequested) {
            finishedSwitching();
        } else {
            effects->addRepaintFull();
        }
    }
    effects->postPaintScreen();
}

#if defined(FACETS_KWIN_NEW_API)
/**
 * @brief ウィンドウ描画の前処理を行う
 * @param view 対象の描画ビュー
 * @param w 対象ウィンドウ
 * @param data ウィンドウ描画の前処理データ
 */
void FacetsEffect::prePaintWindow(RenderView *view, EffectWindow *w, WindowPrePaintData &data)
#else
/**
 * @brief ウィンドウ描画の前処理を行う
 * @param w 対象ウィンドウ
 * @param data ウィンドウ描画の前処理データ
 * @param presentTime 現在の提示時刻
 */
void FacetsEffect::prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime)
#endif
{
    if (m_active && (m_fromWindows.contains(w) || m_toWindows.contains(w) || m_overlayWindows.contains(w))) {
        // オフスクリーンへ描くため変換対象としてマークする
        data.setTransformed();
    }
#if defined(FACETS_KWIN_NEW_API)
    effects->prePaintWindow(view, w, data);
#else
    effects->prePaintWindow(w, data, presentTime);
#endif
}

/**
 * @brief 現在のキャプチャパスに応じて1枚のウィンドウを描画する
 * @param renderTarget 描画先
 * @param viewport ビューポート
 * @param w 対象ウィンドウ
 * @param mask 描画マスク
 * @param region 更新領域
 * @param data ウィンドウ描画データ
 */
#if defined(FACETS_KWIN_NEW_API)
void FacetsEffect::paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const DeviceRegion &region, WindowPaintData &data)
#else
void FacetsEffect::paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
#endif
{
    if (m_pass == Pass::None) {
        // キャプチャ中はパスで選んだウィンドウだけを描き、実描画はKWin側に委ねる
        // 通常描画はそのまま流す
        effects->paintWindow(renderTarget, viewport, w, mask, region, data);
        return;
    }

    bool paint = false;
    switch (m_pass) {
    case Pass::From:
        paint = m_fromWindows.contains(w);
        break;
    case Pass::To:
        paint = m_toWindows.contains(w);
        break;
    case Pass::Background:
        // 壁紙だけを別テクスチャに切り出す
        paint = w->isDesktop() && m_toWindows.contains(w);
        break;
    case Pass::Overlay:
        paint = m_overlayWindows.contains(w);
        break;
    case Pass::None:
        break;
    }
    if (!paint) {
        // このキャプチャパスには含めない
        return;
    }
    effects->paintWindow(renderTarget, viewport, w, mask, region, data);
}

/**
 * @brief アニメーション中かどうかを返す
 * @return 動作中またはジェスチャ追従中ならtrue
 */
bool FacetsEffect::isActive() const
{
    return m_active && !effects->isScreenLocked();
}

/**
 * @brief エフェクトチェーン内の優先位置を返す
 * @return 他のフルスクリーン効果と競合しにくい固定値
 */
int FacetsEffect::requestedEffectChainPosition() const
{
    // 他エフェクトの変形結果を受け取った後に描くため中位の優先順位に固定する
    return 50;
}

/**
 * @brief アニメーションを開始する
 * @param spec 開始条件 (切替元/先・進捗範囲)
 */
void FacetsEffect::startAnimation(const AnimationSpec &spec)
{
    m_fromDesktop = spec.from ? spec.from : effects->currentDesktop();
    m_toDesktop = spec.to ? spec.to : m_fromDesktop;
    m_movingWindow = spec.with;
    m_progressFrom = spec.fromProgress;
    m_targetProgress = spec.toProgress;
    m_progress = spec.fromProgress;
    m_gestureActive = false;
    m_startTimeSet = false;
    m_finishRequested = false;
    m_active = true;

    // グリッド座標の差分から縦横と進行方向を決める
    const QPoint fromCoords = effects->desktopGridCoords(m_fromDesktop);
    const QPoint toCoords = effects->desktopGridCoords(m_toDesktop);
    const int dx = toCoords.x() - fromCoords.x();
    const int dy = toCoords.y() - fromCoords.y();
    m_vertical = (dx == 0) && (dy != 0);
    int sign = m_vertical ? (dy >= 0 ? 1 : -1) : (dx >= 0 ? 1 : -1);
    if (effects->optionRollOverDesktops()) {
        // 端をまたぐ場合は最短方向へ読み替える
        const int gw = std::max(1, effects->desktopGridWidth());
        const int gh = std::max(1, effects->desktopGridHeight());
        if (m_vertical && std::abs(dy) > gh / 2) {
            sign = -sign;
        } else if (!m_vertical && std::abs(dx) > gw / 2) {
            sign = -sign;
        }
    }
    m_direction = sign;

    prepareSwitching();

    effects->setActiveFullScreenEffect(this);
    effects->addRepaintFull();
}

/**
 * @brief 切替前にウィンドウの可視参照を用意する
 */
void FacetsEffect::prepareSwitching()
{
    // 固定オーバーレイの要否を決めてから対象ウィンドウを確定する
    updateOverlayEnabled();
    collectWindows();

    // 対象が変わるので撮影済みのテクスチャを無効にする
    invalidateCaptures();
}

/**
 * @brief 全出力の撮影済みテクスチャを無効にする
 */
void FacetsEffect::invalidateCaptures()
{
    for (auto &entry : m_outputs) {
        entry.second.capturesValid = false;
    }
    if (m_active) {
        effects->addRepaintFull();
    }
}

/**
 * @brief 切替中のウィンドウ構成の変化を反映して撮り直す
 */
void FacetsEffect::refreshWindows()
{
    if (!m_active) {
        return;
    }
    collectWindows();
    invalidateCaptures();
}

/**
 * @brief 切替後の後始末 (参照解放・フルスクリーン効果解除)
 */
void FacetsEffect::finishedSwitching()
{
    m_active = false;
    m_gestureActive = false;
    m_finishRequested = false;
    m_startTimeSet = false;
    m_progress = 0.0;
    m_progressFrom = 0.0;
    m_targetProgress = 1.0;

    m_fromWindows.clear();
    m_toWindows.clear();
    m_overlayWindows.clear();
    // 可視参照を解放して対象デスクトップのウィンドウを通常状態に戻す
    m_visibilityRefs.clear();
    m_movingWindow = nullptr;
    m_fromDesktop = nullptr;
    m_toDesktop = nullptr;

    if (effects->activeFullScreenEffect() == this) {
        effects->setActiveFullScreenEffect(nullptr);
    }
    // 撮影済みの画面ではなく現在のシーンへ確実に戻す
    effects->addRepaintFull();
}

/**
 * @brief 各面に含めるウィンドウ集合を再計算する
 */
void FacetsEffect::collectWindows()
{
    m_fromWindows.clear();
    m_toWindows.clear();
    m_overlayWindows.clear();
    m_visibilityRefs.clear();
    const auto stack = effects->stackingOrder();
    for (EffectWindow *w : stack) {
        classifyWindow(w);
    }
}

/**
 * @brief 固定オーバーレイを出すかどうかを再判定する
 */
void FacetsEffect::updateOverlayEnabled()
{
    m_overlayEnabled = m_keepPanels;
    if (!m_keepPanels) {
        return;
    }
    // 切替元または切替先に属する未削除の全画面ウィンドウがあれば
    // パネルの固定を解除して通常どおり面へ描く
    const auto hasFullScreen = [](VirtualDesktop *desktop) {
        if (!desktop) {
            return false;
        }
        const auto stack = effects->stackingOrder();
        for (EffectWindow *w : stack) {
            if (!w->isDeleted() && w->isFullScreen() && (w->isOnAllDesktops() || w->isOnDesktop(desktop))) {
                return true;
            }
        }
        return false;
    };
    if (hasFullScreen(m_fromDesktop) || hasFullScreen(m_toDesktop)) {
        m_overlayEnabled = false;
    }
}

/**
 * @brief ウィンドウを指定した面の集合へ振り分ける
 * @param w 振り分けるウィンドウ
 */
void FacetsEffect::classifyWindow(EffectWindow *w)
{
    if (w->isDeleted()) {
        return;
    }
    // 切替中に最小化やデスクトップ所属が変わったら対象を取り直す
    connect(w, &EffectWindow::minimizedChanged, this, &FacetsEffect::refreshWindows, Qt::UniqueConnection);
    connect(w, &EffectWindow::windowDesktopsChanged, this, &FacetsEffect::refreshWindows, Qt::UniqueConnection);
    const bool isFixed = w->isDock() || w->isNotification() || w->isCriticalNotification() || w->isOnScreenDisplay();
    const bool toOverlay = m_keepPanels && m_overlayEnabled
        && (w == m_movingWindow || isFixed);
    if (toOverlay) {
        m_overlayWindows.insert(w);
        return;
    }
    const bool onFrom = w->isOnAllDesktops() || w->isOnDesktop(m_fromDesktop);
    const bool onTo = w->isOnAllDesktops() || w->isOnDesktop(m_toDesktop);
    if (onFrom) {
        m_fromWindows.insert(w);
    }
    if (onTo) {
        m_toWindows.insert(w);
    }
    if (onFrom || onTo) {
        // 切替中は非カレントデスクトップのウィンドウも描けるようにする
        m_visibilityRefs.insert(w, EffectWindowVisibleRef(w, EffectWindow::PAINT_DISABLED_BY_DESKTOP));
    }
}

/**
 * @brief グリッド座標にあるデスクトップを返す
 * @param pos グリッド座標
 * @return 見つからなければnullptr
 */
VirtualDesktop *FacetsEffect::desktopAtGrid(const QPoint &pos) const
{
    const auto desktops = effects->desktops();
    for (VirtualDesktop *desktop : desktops) {
        if (effects->desktopGridCoords(desktop) == pos) {
            return desktop;
        }
    }
    return nullptr;
}

/**
 * @brief デスクトップ切替の確定をアニメーション開始へ変換する
 * @param oldDesktop 切替元デスクトップ
 * @param newDesktop 切替先デスクトップ
 * @param with 一緒に移動するウィンドウ
 */
void FacetsEffect::desktopChanged(VirtualDesktop *oldDesktop, VirtualDesktop *newDesktop, EffectWindow *with)
{
    if (effects->isScreenLocked() || (effects->hasActiveFullScreenEffect() && effects->activeFullScreenEffect() != this)) {
        // ロック中、または他のフルスクリーン効果があるときは譲る
        return;
    }
    if (animationDuration().count() <= 0) {
        // アニメーション無効時は何もせず終了させる
        finishedSwitching();
        return;
    }

    AnimationSpec spec;
    spec.from = oldDesktop ? oldDesktop : effects->currentDesktop();
    spec.to = newDesktop ? newDesktop : effects->currentDesktop();
    spec.with = with;
    spec.fromProgress = 0.0;
    spec.toProgress = 1.0;

    if (m_active) {
        if (m_gestureActive && m_fromDesktop == spec.from && m_toDesktop == spec.to) {
            // ジェスチャ確定: 指の位置から残りを流す
            m_gestureActive = false;
            m_progressFrom = m_gestureProgress;
            m_targetProgress = 1.0;
            m_progress = m_progressFrom;
            m_startTimeSet = false;
            m_finishRequested = false;
            effects->addRepaintFull();
            return;
        }
        // アニメーション中の再切替: いま主に見えているデスクトップから再開する
        VirtualDesktop *visible = (m_progress < 0.5) ? m_fromDesktop : m_toDesktop;
        if (visible) {
            spec.from = visible;
        }
        if (spec.from == spec.to) {
            // 見えているデスクトップが目的地なら即座に確定する
            finishedSwitching();
            return;
        }
    }
    startAnimation(spec);
}

/**
 * @brief スワイプ中のデスクトップ切替に追従させる
 * @param currentDesktop 現在のデスクトップ
 * @param offset スワイプ量
 * @param with 一緒に移動するウィンドウ
 */
void FacetsEffect::desktopChanging(VirtualDesktop *currentDesktop, QPointF offset, EffectWindow *with)
{
    if (effects->isScreenLocked() || (effects->hasActiveFullScreenEffect() && effects->activeFullScreenEffect() != this)) {
        return;
    }
    // 支配的な軸と向きを求める
    const bool vertical = std::abs(offset.y()) > std::abs(offset.x());
    const qreal amount = vertical ? offset.y() : offset.x();
    const int dir = amount < 0.0 ? -1 : 1;

    // KWinの確定処理と同じ規則で到来面を求める(端ではrolloverを考慮)
    VirtualDesktop *from = currentDesktop ? currentDesktop : effects->currentDesktop();
    const bool wrap = effects->optionRollOverDesktops();
    VirtualDesktop *to = nullptr;
    if (vertical) {
        to = dir < 0 ? effects->desktopAbove(from, wrap) : effects->desktopBelow(from, wrap);
    } else {
        to = dir < 0 ? effects->desktopToLeft(from, wrap) : effects->desktopToRight(from, wrap);
    }
    if (!to) {
        to = from;
    }
    if (m_active && m_gestureActive && m_fromDesktop == from && m_toDesktop == to && m_vertical == vertical && m_direction == dir) {
        // 同じ対象の追従中は進捗だけを更新する
        m_movingWindow = with;
        m_gestureProgress = std::clamp(std::abs(amount), 0.0, 1.0);
        m_progress = m_gestureProgress;
        effects->addRepaintFull();
        return;
    }

    m_fromDesktop = from;
    m_toDesktop = to;
    m_movingWindow = with;
    m_vertical = vertical;
    m_direction = dir;
    m_active = true;
    m_gestureActive = true;
    m_startTimeSet = false;
    m_finishRequested = false;
    m_progressFrom = 0.0;
    m_targetProgress = 1.0;
    m_gestureProgress = std::clamp(std::abs(amount), 0.0, 1.0);
    m_progress = m_gestureProgress;

    prepareSwitching();

    effects->setActiveFullScreenEffect(this);
    effects->addRepaintFull();
}

/**
 * @brief スワイプが取り消されたら元の進捗へ戻す
 */
void FacetsEffect::desktopChangingCancelled()
{
    if (!m_active || effects->activeFullScreenEffect() != this) {
        return;
    }
    // 向きと面を保ったまま、指を離した位置から元の進捗0へ戻す
    m_gestureActive = false;
    m_progressFrom = m_gestureProgress;
    m_targetProgress = 0.0;
    m_progress = m_progressFrom;
    m_startTimeSet = false;
    m_finishRequested = false;
    effects->addRepaintFull();
}

#ifdef FACETS_HAVE_DESKTOP_MOVED
/**
 * @brief デスクトップの並び替え通知 (進行中のアニメーションは崩さない)
 * @param desktop 対象デスクトップ
 * @param position 新しい位置
 */
void FacetsEffect::desktopMoved(VirtualDesktop *desktop, int position)
{
    Q_UNUSED(desktop)
    Q_UNUSED(position)
    // 実行中でもキャプチャ済みの状態で最後まで流す(方向のみ固定済み)
}
#endif

/**
 * @brief 切替中に追加されたウィンドウを対象へ振り分ける
 * @param w 追加されたウィンドウ
 */
void FacetsEffect::windowAdded(EffectWindow *w)
{
    if (!m_active) {
        return;
    }
    classifyWindow(w);
    invalidateCaptures();
}

/**
 * @brief 削除されたウィンドウを各集合から取り除く
 * @param w 削除されたウィンドウ
 */
void FacetsEffect::windowDeleted(EffectWindow *w)
{
    m_fromWindows.remove(w);
    m_toWindows.remove(w);
    m_overlayWindows.remove(w);
    m_visibilityRefs.remove(w);
    if (m_movingWindow == w) {
        m_movingWindow = nullptr;
    }
    if (m_active) {
        invalidateCaptures();
    }
}

/**
 * @brief 出力追加時にGLリソースを作り直す
 * @param output 追加された出力
 */
void FacetsEffect::outputAdded(ScreenOutput *output)
{
    Q_UNUSED(output)
    // 出力構成が変わったらGLリソースを作り直す
    if (m_active) {
        finishedSwitching();
    }
    if (effects) {
        effects->makeOpenGLContextCurrent();
    }
    m_outputs.clear();
}

/**
 * @brief 出力削除時に該当のGLリソースを破棄する
 * @param output 削除された出力
 */
void FacetsEffect::outputRemoved(ScreenOutput *output)
{
    if (m_active) {
        finishedSwitching();
    }
    if (effects) {
        effects->makeOpenGLContextCurrent();
    }
    m_outputs.erase(output);
}

} // 名前空間の終わり
