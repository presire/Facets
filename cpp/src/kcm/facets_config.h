/*
    SPDX-FileCopyrightText: 2026 facets contributors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_facets_config.h"

#include <KCModule>

namespace KWin
{

/**
 * @class FacetsEffectConfig
 * @brief facetsエフェクトの設定モジュール
 *
 * システム設定の「アニメーション」KCMから開かれる設定フォームを提供し
 * 保存時にエフェクトへ設定の再読み込みを要求する
 */
class FacetsEffectConfig : public KCModule
{
    Q_OBJECT

public:
    /**
     * @brief コンストラクタ
     * @param parent 親オブジェクト
     * @param data プラグインメタデータ
     */
    explicit FacetsEffectConfig(QObject *parent, const KPluginMetaData &data);

    /** @brief デストラクタ */
    ~FacetsEffectConfig() override;

    /** @brief 設定を保存しエフェクトの再読み込みを要求する */
    void save() override;

private:
    ::Ui::FacetsConfig m_ui; ///< 生成された設定フォーム
};

} // 名前空間の終わり
