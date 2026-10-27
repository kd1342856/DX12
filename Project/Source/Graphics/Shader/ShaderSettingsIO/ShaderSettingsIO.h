#pragma once
#include <string>
#include <vector>

//=============================
//
// シェーダーの設定(ShaderManagerが持つRenderer・Lighting・Shadow・PostProcess・SSAO・Fog)のJSONへの読み書き
//
//   保存する値は、各設定の構造体のVisitFieldsに並べたもの。JSONは
//     { "Renderer": { "SSRStepSize": 0.35, ... }, "Lighting": { ... }, ... }
//   の形で、シーンのJSON(ルートに同じ形で入る)、プリセット、A/B比較で共通に使う。
//
//=============================
namespace ShaderSettingsIO
{
    // 今の設定をJSONにする
    nlohmann::json Capture();
    // 初期値(構造体の既定値)をJSONにする
    nlohmann::json CaptureDefaults();
    // JSONにある値だけを設定に入れ、そのグループのIsDirtyを立てる。JSONに無いキーは今の値のまま
    void Apply(const nlohmann::json& json);

    // 値1つの読み書き(Undo/Redo用)。保存しない値(画質設定などが決める値)も扱う。見つからなければnull・false
    nlohmann::json CaptureField(const std::string& group, const std::string& key);
    bool ApplyField(const std::string& group, const std::string& key, const nlohmann::json& value);
    // 値の表示名(VisitFieldsの表示名)。見つからなければキーのまま
    std::string GetFieldLabel(const std::string& group, const std::string& key);

    // 保存していない変更。シーンを読み込んだとき・保存したときの値を控え、今の値と比べる
    void MarkSaved();
    int CountUnsavedChanges();

    // 2つのJSONで値が違うキーの数
    int CountDifferences(const nlohmann::json& lhs, const nlohmann::json& rhs);

    // プリセット: Asset/Data/ShaderPreset/<名前>.json。名前はUTF-8
    std::vector<std::string> ListPresets();
    bool HasPreset(const std::string& name);
    bool SavePreset(const std::string& name, std::string& outError);
    bool LoadPreset(const std::string& name, std::string& outError);
    // プリセットの説明(JSONの "_description")。プリセットは一部のグループ・値だけを持ってもよく、無い値は今のまま
    std::string GetPresetDescription(const std::string& name);
}
