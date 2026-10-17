/*
    facetsエフェクトプラグインのエントリポイント

    KWin6系のエフェクトはEffectPluginFactoryをKPluginFactoryとして公開する
    KWIN_EFFECT_FACTORY_SUPPORTEDがその定型クラスを生成する
    [Id]/名称などのメタデータはmetadata.jsonから取り込まれる
*/

#include "facets.h"

#include <effect/effect.h>

namespace KWin
{

// FacetsEffectをKWinのエフェクトとして登録する
// 第3引数は実行環境が対応しているかの判定
KWIN_EFFECT_FACTORY_SUPPORTED(FacetsEffect, "metadata.json",
                              return FacetsEffect::supported();)

} // 名前空間の終わり

#include "main.moc"
