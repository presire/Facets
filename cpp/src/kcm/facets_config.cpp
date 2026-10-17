/*
    SPDX-FileCopyrightText: 2026 facets contributors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "facets_config.h"

// KConfigSkeleton (生成された設定クラス)
#include "facetsconfig.h"

#include <KPluginFactory>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QString>

K_PLUGIN_CLASS(KWin::FacetsEffectConfig)

namespace KWin
{

// KWinが使うメイン設定ファイル名
static constexpr QLatin1String KWIN_CONFIG("kwinrc");

/**
 * @brief コンストラクタ
 * @param parent 親オブジェクト
 * @param data プラグインメタデータ
 */
FacetsEffectConfig::FacetsEffectConfig(QObject *parent, const KPluginMetaData &data)
    : KCModule(parent, data)
{
    m_ui.setupUi(widget());
    FacetsConfig::instance(KWIN_CONFIG);
    addConfig(FacetsConfig::self(), widget());
}

/** @brief デストラクタ */
FacetsEffectConfig::~FacetsEffectConfig() = default;

/**
 * @brief 設定を保存しエフェクトの再読み込みを要求する
 */
void FacetsEffectConfig::save()
{
    KCModule::save();

    // 設定変更をKWinのエフェクトへ即時反映させる
    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"),
                                                          QStringLiteral("/Effects"),
                                                          QStringLiteral("org.kde.kwin.Effects"),
                                                          QStringLiteral("reconfigureEffect"));
    message << QStringLiteral("facets");
    QDBusConnection::sessionBus().send(message);
}

} // 名前空間の終わり

#include "facets_config.moc"

#include "moc_facets_config.cpp"
