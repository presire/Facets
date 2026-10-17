/*
    Facets virtual desktop switching animation for KWin (Plasma 6, X11 and Wayland).

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/**
 * @file main.qml
 * @brief KWinのシーンエフェクトとして3D仮想デスクトップ切り替えを統括するルートコンポーネント
 *
 * KWinから読み込まれる[SceneEffect]を継承し、[Workspace]の[currentDesktopChanged]を監視する
 * 切り替え1区間を画面ごとの[ScreenView]へ委譲してアニメーションさせ、区間の再生中にさらに
 * 切り替えが起きた場合は[pendingDesktop]へ最新の切り替え先を保持し、連続する区間として続けて再生する
 *
 * 入力 : [effect.configuration] (KConfig XTの[main.xml]由来の設定)、
 *        KWinComponentsの[Workspace]と[Options]
 * 出力 : [delegate]として生成する[ScreenView]群、[progress]などの共有状態
 * 依存 : KWinComponents ([SceneEffect]、[Workspace]、[Options])、
 *        Kirigami.Units (既定時間)、[constants.js]
 */
import QtQuick
import org.kde.kirigami as Kirigami
import org.kde.kwin as KWinComponents

import "constants.js" as Constants

KWinComponents.SceneEffect {
    id: effect

    // アニメーション対象のデスクトップで、画面ごとのデリゲートへ公開する
    property QtObject fromDesktop: null        // 切り替え元のデスクトップ
    property QtObject toDesktop: null          // 切り替え先のデスクトップ
    // デスクトップ格子内での移動方向 (dx・dyのどちらか一方だけが非ゼロ)
    // dx: +1 = 新しいデスクトップが右、dy: +1 = 下
    property int dx: 0                         // 横方向の移動単位
    property int dy: 0                         // 縦方向の移動単位
    property int generation: 0                 // 区間ごとに加算し、デリゲートへ区間状態の再読込を促す
    // 全スクリーンで同期するための共有アニメーション時計
    property real progress: 0                  // 0.0-1.0の進行度
    // 区間の再生中にユーザが切り替えた先のデスクトップで、即座に飛ばず後続区間として連続的に動かす
    property QtObject pendingDesktop: null     // 再生待ちの次のデスクトップ
    property int readyViews: 0                 // 読み込みが完了した画面ビューの数 (ビューは非同期に生成される)
    property bool waitingForViews: false       // 全ビューの読み込みを待っているかどうか

    readonly property int style: effect.configuration.Style ?? Constants.Style.Cube  // [Style]設定、既定は[Constants.Style.Cube]
    // [Duration]設定の値 (ミリ秒)、0ならKirigamiの長時間アニメーション基準から算出する
    readonly property int duration: {                                                // [Duration]設定から求める再生時間(ミリ秒)
        const configured = effect.configuration.Duration ?? 0;
        if (configured > 0) {
            return configured;
        }
        return Math.round(Kirigami.Units.longDuration * 2.4);
    }
    property bool animating: false                                                   // 区間を再生中かどうか

    delegate: Component {
        ScreenView {}
    }

    /**
     * @brief デスクトップの格子座標を求める
     * @param desktop 対象の仮想デスクトップ
     * @return column・rowと有効性validを持つオブジェクト、未登録ではvalidがfalse
     */
    function gridPosition(desktop) {
        const index = KWinComponents.Workspace.desktops.indexOf(desktop);
        const columns = Math.max(1, KWinComponents.Workspace.desktopGridWidth);
        return { column: index % columns, row: Math.floor(index / columns), valid: index >= 0 };
    }

    /**
     * @brief リング上での最短の符号付き距離を求める
     * @param delta 生の差分 (列または行)
     * @param size リングの要素数
     * @return 最短経路となる符号付き差分
     * @note [KWinComponents.Options.rollOverDesktops]が有効なときだけ折り返す
     *       末尾から先頭への折り返しを1ステップの前進として動かすため (標準のSlideエフェクトと同様)
     */
    function wrappedDelta(delta, size) {
        if ((KWinComponents.Options.rollOverDesktops ?? true) && size > 2 && Math.abs(delta) > size / 2) {
            return delta - Math.sign(delta) * size;
        }
        return delta;
    }

    /**
     * @brief ウィンドウのドラッグ移動・リサイズ操作が進行中か判定する
     * @return 進行中ならtrue
     */
    function interactiveMoveResizeActive() {
        return KWinComponents.Workspace.stackingOrder.some(window => window && (window.move || window.resize));
    }

    /**
     * @brief 直前のデスクトップから現在への1区間のアニメーションを開始する
     * @param previous 切り替え元のデスクトップ
     * @param current 切り替え先のデスクトップ
     * @return 開始したらtrue、動かすものがない場合はfalse
     * @note [progress]・[dx]・[dy]・[fromDesktop]・[toDesktop]を更新し、
     *       全ビューの準備が整うか、[viewsFallbackTimer]の待機期限に達したところで[beginProgress]を呼ぶ
     */
    function startSegment(previous, current) {
        const from = gridPosition(previous);
        const to = gridPosition(current);
        if (!from.valid || !to.valid) {
            return false;
        }

        const columns = Math.max(1, KWinComponents.Workspace.desktopGridWidth);
        const rows = Math.max(1, KWinComponents.Workspace.desktopGridHeight);
        const deltaColumn = wrappedDelta(to.column - from.column, columns);
        const deltaRow = wrappedDelta(to.row - from.row, rows);

        if (deltaColumn === 0 && deltaRow === 0) {
            return false;
        }
        if (Math.abs(deltaColumn) >= Math.abs(deltaRow)) {
            dx = Math.sign(deltaColumn);
            dy = 0;
        } else {
            dx = 0;
            dy = Math.sign(deltaRow);
        }

        progressAnimation.stop();
        progress = 0;
        fromDesktop = previous;
        toDesktop = current;
        generation += 1;
        animating = true;

        if (!visible) {
            visible = true;
        } else if (readyViews === 0) {
            // 表示中でもビューが無い場合: 別の全画面エフェクトが動作中で先の開始が拒否されている
            // visibleを一度落として再度上げ、開始をリトライする
            visible = false;
            visible = true;
        }

        if (readyViews >= KWinComponents.Workspace.screens.length) {
            beginProgress();
        } else {
            // 非同期生成のビューが揃うまで待ち、フレーム落ちを防ぐ
            waitingForViews = true;
            viewsFallbackTimer.restart();
        }
        safetyTimer.restart();
        return true;
    }

    /**
     * @brief 全ビューの準備が整うか、150msの待機期限に達したところで進行アニメーションを開始する
     * @note [waitingForViews]を解除し、[viewsFallbackTimer]を停止する
     */
    function beginProgress() {
        waitingForViews = false;
        viewsFallbackTimer.stop();
        progressAnimation.restart();
    }

    /**
     * @brief 画面ビューの読み込み完了を登録する (ScreenViewの生成完了に対応)
     * @note [readyViews]を増やす
     * @note 全スクリーン分が揃っていれば[beginProgress]を呼ぶ
     */
    function viewReady() {
        readyViews += 1;
        if (waitingForViews && readyViews >= KWinComponents.Workspace.screens.length) {
            beginProgress();
        }
    }

    /**
     * @brief 画面ビューの破棄を登録する
     * @note [readyViews]を0未満にしない範囲で減らす
     */
    function viewGone() {
        readyViews = Math.max(0, readyViews - 1);
    }

    /**
     * @brief エフェクト全体を終了する (アニメーション完了・キー入力・クリック・キャンセル)
     * @note [fromDesktop]・[toDesktop]は[visible]をfalseにした直後の最終フレーム描画のため保持する
     */
    function finish() {
        progressAnimation.stop();
        viewsFallbackTimer.stop();
        safetyTimer.stop();
        waitingForViews = false;
        pendingDesktop = null;
        animating = false;
        visible = false;
    }

    /**
     * @brief 1区間の再生終了時の処理
     * @note 待機中の[pendingDesktop]があれば後続区間として続け、なければ[finish]する
     */
    function segmentFinished() {
        const next = pendingDesktop;
        pendingDesktop = null;
        if (next && next !== toDesktop && next === KWinComponents.Workspace.currentDesktop && startSegment(toDesktop, next)) {
            return;
        }
        finish();
    }

    // 共有時計[progress]を0から1へ動かし、[duration]を再生時間に使う
    // [Deck]のときだけ減速カーブ (OutCubic)とし、他のスタイルは加減速 (InOutCubic)にする
    NumberAnimation {
        id: progressAnimation
        target: effect
        property: "progress"
        from: 0
        to: 1
        duration: effect.duration
        easing.type: effect.style === Constants.Style.Deck ? Easing.OutCubic : Easing.InOutCubic
        onFinished: effect.segmentFinished()
    }

    // 非同期生成されるビューが一定時間で揃わない場合のフォールバック
    // 150ms待っても全ビューが揃わなければ待機をやめて再生を始める
    Timer {
        id: viewsFallbackTimer
        interval: 150
        onTriggered: effect.beginProgress()
    }

    // エフェクトが上がりっぱなしにならないことを保証する
    // OverviewやCubeなど別の全画面エフェクトに妨げられてシーンが一度も表示されなかった場合などに備える
    Timer {
        id: safetyTimer
        interval: effect.duration + 1000
        onTriggered: effect.finish()
    }

    // KWinの[Workspace]からのシグナルで区間を駆動する
    Connections {
        target: KWinComponents.Workspace

        /**
         * @brief 現在デスクトップの切り替えを受けて切り替え区間を開始する
         * @param previous 切り替え元のデスクトップ
         * @note 再生中は[pendingDesktop]へ最新の切り替え先を保持して後続区間とし、ウィンドウの移動・リサイズ操作中は入力を奪わない
         */
        function onCurrentDesktopChanged(previous) {
            if (!previous || effect.duration <= 0) {
                return;
            }
            if (effect.animating) {
                effect.pendingDesktop = KWinComponents.Workspace.currentDesktop;  // 再生中は最新の切り替え先を後続区間として保持する
                return;
            }
            if (effect.interactiveMoveResizeActive()) {
                // デスクトップをまたいでウィンドウをドラッグ中の入力は奪わない
                return;
            }
            effect.startSegment(previous, KWinComponents.Workspace.currentDesktop);
        }

        /**
         * @brief アクティビティ切り替え時に表示中のエフェクトを終了する
         * @note アクティビティ切り替えは先にデスクトップを復元するため、表示中のウィンドウと食い違う
         */
        function onCurrentActivityChanged() {
            if (effect.visible) {
                effect.finish();
            }
        }
        /**
         * @brief デスクトップの追加・削除時に表示中のエフェクトを終了する
         */
        function onDesktopsChanged() {
            if (effect.visible) {
                effect.finish();
            }
        }
        /**
         * @brief デスクトップ格子の構成変更時に表示中のエフェクトを終了する
         */
        function onDesktopLayoutChanged() {
            if (effect.visible) {
                effect.finish();
            }
        }
    }
}
