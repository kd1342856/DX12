#pragma once

//=============================
//
// エディターの設定(人ごとの見え方など)。シーンやプリセットではなく <root>/Saved/EditorSettings.json に保存する
//
//   今は表示ガンマだけ。よく使う値のピン留めなども、ここに足していく
//
//=============================
namespace EditorPreferences
{
    // 起動時に1回呼ぶ。ファイルが無ければ既定値のまま
    void Load();
    void Save();
}
