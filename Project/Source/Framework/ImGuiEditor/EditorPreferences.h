#pragma once
#include <string>
#include <vector>

//=============================
//
// エディターの設定(人ごとの見え方など)。シーンやプリセットではなく <root>/Saved/EditorSettings.json に保存する
//
//   表示ガンマと、よく使う値のピン留め(人ごとに違うので、シーンには入れない)
//
//=============================
namespace EditorPreferences
{
    // 起動時に1回呼ぶ。ファイルが無ければ既定値のまま
    void Load();
    void Save();

    // よく使う値。対象(PostProcessなど)と値のキーの組を、並べた順に持つ
    struct Pin
    {
        std::string target;
        std::string key;
    };
    const std::vector<Pin>& GetPins();
    bool IsPinned(const std::string& target, const std::string& key);
    // 入っていれば外し、無ければ最後に足す(保存もする)
    void TogglePin(const std::string& target, const std::string& key);
    // 並べ替え(-1で上へ、+1で下へ)。保存もする
    void MovePin(const std::string& target, const std::string& key, int offset);
}
