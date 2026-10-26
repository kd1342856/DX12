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

    // 2つのJSONで値が違うキーの数
    int CountDifferences(const nlohmann::json& lhs, const nlohmann::json& rhs);

    // プリセット: Asset/Data/ShaderPreset/<名前>.json。名前はUTF-8
    std::vector<std::string> ListPresets();
    bool HasPreset(const std::string& name);
    bool SavePreset(const std::string& name, std::string& outError);
    bool LoadPreset(const std::string& name, std::string& outError);
}
