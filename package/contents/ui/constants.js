/**
 * @file constants.js
 * @brief QML側とKConfig XT設定の間で共有する定数の定義
 *
 * [config/main.xml]の[Style]列挙体、および[config.ui]の[kcfg_Style]コンボボックスの
 * 並び順と一致させる必要がある
 *
 * 入力 : なし
 * 出力 : [Style]列挙体 (Cube / Flip / Deck)
 * 依存 : なし (.pragma libraryとして各QMLからimportされる)
 */
.pragma library

// [config/main.xml]の[Style]列挙体と[config.ui]の[kcfg_Style]コンボボックスに対応する
const Style = {
    Cube: 0,   // キューブ: 隣接する2面を箱の回転で切り替える
    Flip: 1,   // フリップ: カードを裏返すように前後面を切り替える
    Deck: 2,   // デッキ: 旧デスクトップが奥へ退き、新デスクトップが手前に重なる
};
