#pragma once
#include <cstdint>
#include "nlohmann/json.hpp"
#include "Material.h"

//=============================
//
// マテリアルの値の上書き(モデルの.metaに保存する層)
//
//   マテリアルの値は「glTFの値(Material::ImportedConstants) → .metaの上書き → (将来)置いた物ごとの上書き」の
//   層で決める。上書きは、値を比べて決めるのではなく、フィールドごとの「上書き中」のフラグで持つ
//   (同じ値に戻しても上書きのまま、「初期値に戻す」でフラグを外す)。
//   エディターで変えられるのは、描き方(PSO)に関わらない値だけ。
//
//=============================
enum class MaterialField : uint8_t
{
    BaseColor,          // 色(RGB)
    Opacity,            // 不透明度(baseColorFactorのA)
    Metallic,
    Roughness,
    NormalScale,
    OcclusionStrength,
    EmissiveColor,      // 発光の色(RGB)
    EmissiveStrength,   // 発光の輝度(nit)
    EmissiveFrontScale,
    EmissiveBackScale,
    Count,
};

namespace MaterialFields
{
    constexpr int kCount = static_cast<int>(MaterialField::Count);

    // JSONのキー(.metaとUndo/Redoで使う。保存したファイルとの互換のため変えないこと)
    const char* GetKey(MaterialField field);
    // キーからフィールドを探す。見つからなければfalse
    bool FindByKey(const std::string& key, MaterialField& outField);
    // 色(RGB)の値か。そうでなければfloat1つ
    bool IsColor(MaterialField field);

    float GetFloat(const MaterialConstants& constants, MaterialField field);
    void SetFloat(MaterialConstants& constants, MaterialField field, float value);
    DirectX::XMFLOAT3 GetColor(const MaterialConstants& constants, MaterialField field);
    void SetColor(MaterialConstants& constants, MaterialField field, const DirectX::XMFLOAT3& value);

    // 値1つをsourceからdestinationへ写す
    void Copy(MaterialConstants& destination, const MaterialConstants& source, MaterialField field);

    // 値1つをJSONにする(数値か[r, g, b])
    nlohmann::json ToJson(const MaterialConstants& constants, MaterialField field);
    // JSONから値1つを書く。形が合わなければfalse(書かない)
    bool FromJson(const nlohmann::json& json, MaterialConstants& constants, MaterialField field);
}

struct MaterialOverride
{
    uint32_t mask = 0;         // 上書き中のフィールド(1 << MaterialField)
    MaterialConstants values;  // 上書きの値(maskのフィールドだけが意味を持つ)

    bool Has(MaterialField field) const { return (mask & (1u << static_cast<uint32_t>(field))) != 0; }
    bool IsEmpty() const { return mask == 0; }
    // 上書きを入れる(値はsourceから写す)
    void Set(MaterialField field, const MaterialConstants& source);
    void Clear(MaterialField field) { mask &= ~(1u << static_cast<uint32_t>(field)); }

    // glTFの値(ImportedConstants)に上書きを重ねて、material.Constantsの編集できる値を決め直す。
    // ほかの値(反射の枠・ディゾルブなど、実行中に変わる値)には触らない
    void ApplyTo(Material& material) const;
};
