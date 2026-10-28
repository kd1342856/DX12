#pragma once
#include <filesystem>
#include <string>
#include "nlohmann/json.hpp"

//=============================
//
// アセットの.metaファイル(モデル・テクスチャなどの横に置く設定)の共通の読み書き
//
//   <アセットのパス>.meta に、キーの順序を固定して整形したJSONで書く(gitの差分とマージを読みやすくする)。
//   先頭の "version" で形式の版を持つ。今より新しい版のファイルは読まず、上書きもしない(内容を壊さないため)。
//   中身の形は種類ごと(モデルならModelMetaManager)に決め、知らないキーは警告して読み飛ばし、保存では残す。
//
//=============================
namespace AssetMeta
{
    // 今の形式の版
    constexpr int kVersion = 1;

    enum class LoadResult
    {
        NotFound,     // .metaが無い(空のJSONを返す)
        Loaded,
        Broken,       // 読めない・JSONでない(上書きしないこと)
        NewerVersion, // 今より新しい版(上書きしないこと)
    };

    std::filesystem::path GetPath(const std::string& assetPath);

    // 読む。Loaded以外ではoutJsonは空のオブジェクト。問題があればログに出す
    LoadResult Load(const std::string& assetPath, nlohmann::ordered_json& outJson);

    // 書く。"version"は先頭に付け直す。書きかけを読まないよう、別の名前で書いてから置き換える
    bool Save(const std::string& assetPath, const nlohmann::ordered_json& json);

    // .metaを消す(上書きが1つも無くなったとき)。無ければ何もしない
    bool Remove(const std::string& assetPath);
}
