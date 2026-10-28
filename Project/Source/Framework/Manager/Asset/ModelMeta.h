#pragma once
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include "nlohmann/json.hpp"
#include "../../../Graphics/GPUResource/Material/MaterialOverride.h"

//=============================
//
// モデルの.meta(エディターで変えたマテリアルの値)
//
//   <モデル>.meta の "materials" に、glTFのマテリアルごとの上書きを並べる。
//     { "name": "Wood_Floor", "index": 2, "overrides": { "roughnessFactor": 0.65 } }
//   名前と番号の両方を持ち、読み込むときは「名前が一意ならそれで引く → 引けなければ、名前が合う(か名前が無い)ときだけ番号で引く
//   → 名前と番号が食い違ったら警告」とする。どれにも当たらないエントリは使わずに残し、保存しても消さない
//   (glTFを一時的に壊して戻したときに設定を失わないため)。
//   上書きはモデル単位で、同じモデルを置いた物すべてに効く。
//
//=============================
class ModelMetaManager
{
public:
    static ModelMetaManager& Instance();

    // モデルを読み込むときに呼ぶ(ワーカースレッドから呼んでよい)。glTFのマテリアルの名前を、glTFの中の順に渡す。
    // 保存していない変更があるモデルは、ファイルを読み直さず、今の内容を新しい番号に結び付け直す
    void LoadForModel(const std::string& modelPath, const std::vector<std::string>& materialNames);

    // glTFのマテリアルの番号の上書き(無ければ空)
    MaterialOverride GetOverride(const std::string& modelPath, int sourceIndex) const;
    // 上書きを置き換える(エディターから)。保存していない変更になる
    void SetOverride(const std::string& modelPath, int sourceIndex, const MaterialOverride& materialOverride);

    // glTFのマテリアルの名前(読み込んだときのもの)。分からなければ空
    std::string GetMaterialName(const std::string& modelPath, int sourceIndex) const;
    // どのマテリアルにも当たらなかったエントリの数
    int CountUnmatched(const std::string& modelPath) const;
    // 新しい版や壊れたファイルなので、保存できない
    bool IsReadOnly(const std::string& modelPath) const;

    bool IsDirty(const std::string& modelPath) const;
    // 保存していない変更があるモデルの数
    int CountDirtyModels() const;
    bool Save(const std::string& modelPath);
    // 保存していない変更があるモデルをすべて保存する。1つでも失敗したらfalse
    bool SaveAll();

private:
    ModelMetaManager() = default;

    struct Entry
    {
        std::string name;
        int index = -1;
        MaterialOverride materialOverride;
        nlohmann::ordered_json unknownOverrides = nlohmann::ordered_json::object(); // 知らないキー(保存で残す)
        int resolvedIndex = -1; // 今のglTFで当たった番号。-1は当たらない
    };

    struct Model
    {
        std::vector<std::string> materialNames;
        std::vector<Entry> entries;
        nlohmann::ordered_json unknownTopLevel = nlohmann::ordered_json::object(); // 知らない最上位のキー(保存で残す)
        bool isLoaded = false;
        bool isDirty = false;
        bool isReadOnly = false;
        bool hasFile = false;
    };

    void ParseFile(const std::string& modelPath, const nlohmann::ordered_json& json, Model& model) const;
    void Resolve(const std::string& modelPath, Model& model) const;
    Entry* FindEntry(Model& model, int sourceIndex);
    const Entry* FindEntry(const Model& model, int sourceIndex) const;
    bool SaveLocked(const std::string& modelPath, Model& model);

    mutable std::mutex m_mutex;
    std::unordered_map<std::string, Model> m_models;
};
