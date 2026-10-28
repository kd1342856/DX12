#include "../../../Pch.h"
#include "MaterialOverride.h"
#include <charconv>

namespace
{
    // floatÇJSONÇ…èëÇ≠Ç∆0.6499999761581421ÇÃÇÊÇ§Ç»åÖÇ…Ç»ÇÈÇÃÇ≈ÅAfloatÇ∆ÇµÇƒç≈íZÇÃï\ãL(0.65)ÇÃdoubleÇ…Ç∑ÇÈ
    double ToShortestDouble(float value)
    {
        char buffer[32] = {};
        const std::to_chars_result result = std::to_chars(buffer, buffer + sizeof(buffer), value);
        if (result.ec != std::errc())
        {
            return static_cast<double>(value);
        }
        return std::strtod(std::string(buffer, result.ptr).c_str(), nullptr);
    }

    // MaterialFieldÇ∆ìØÇ∂èá
    const char* const kKeys[MaterialFields::kCount] =
    {
        "baseColor",
        "opacity",
        "metallicFactor",
        "roughnessFactor",
        "normalScale",
        "occlusionStrength",
        "emissiveColor",
        "emissiveStrength",
        "emissiveFrontScale",
        "emissiveBackScale",
    };

    float* FindFloat(MaterialConstants& constants, MaterialField field)
    {
        switch (field)
        {
            case MaterialField::Opacity:            return &constants.baseColorFactor.w;
            case MaterialField::Metallic:           return &constants.metallicFactor;
            case MaterialField::Roughness:          return &constants.roughnessFactor;
            case MaterialField::NormalScale:        return &constants.normalScale;
            case MaterialField::OcclusionStrength:  return &constants.occlusionStrength;
            case MaterialField::EmissiveStrength:   return &constants.emissiveStrength;
            case MaterialField::EmissiveFrontScale: return &constants.emissiveFrontScale;
            case MaterialField::EmissiveBackScale:  return &constants.emissiveBackScale;
            default:                                return nullptr;
        }
    }
}

const char* MaterialFields::GetKey(MaterialField field)
{
    return kKeys[static_cast<int>(field)];
}

bool MaterialFields::FindByKey(const std::string& key, MaterialField& outField)
{
    for (int fieldIndex = 0; fieldIndex < kCount; ++fieldIndex)
    {
        if (key == kKeys[fieldIndex])
        {
            outField = static_cast<MaterialField>(fieldIndex);
            return true;
        }
    }
    return false;
}

bool MaterialFields::IsColor(MaterialField field)
{
    return field == MaterialField::BaseColor || field == MaterialField::EmissiveColor;
}

float MaterialFields::GetFloat(const MaterialConstants& constants, MaterialField field)
{
    const float* pValue = FindFloat(const_cast<MaterialConstants&>(constants), field);
    return pValue ? *pValue : 0.0f;
}

void MaterialFields::SetFloat(MaterialConstants& constants, MaterialField field, float value)
{
    if (float* pValue = FindFloat(constants, field))
    {
        *pValue = value;
    }
}

DirectX::XMFLOAT3 MaterialFields::GetColor(const MaterialConstants& constants, MaterialField field)
{
    if (field == MaterialField::BaseColor)
    {
        return { constants.baseColorFactor.x, constants.baseColorFactor.y, constants.baseColorFactor.z };
    }
    if (field == MaterialField::EmissiveColor)
    {
        return { constants.emissiveFactorX, constants.emissiveFactorY, constants.emissiveFactorZ };
    }
    return { 0.0f, 0.0f, 0.0f };
}

void MaterialFields::SetColor(MaterialConstants& constants, MaterialField field, const DirectX::XMFLOAT3& value)
{
    if (field == MaterialField::BaseColor)
    {
        constants.baseColorFactor.x = value.x;
        constants.baseColorFactor.y = value.y;
        constants.baseColorFactor.z = value.z;
    }
    else if (field == MaterialField::EmissiveColor)
    {
        constants.emissiveFactorX = value.x;
        constants.emissiveFactorY = value.y;
        constants.emissiveFactorZ = value.z;
    }
}

void MaterialFields::Copy(MaterialConstants& destination, const MaterialConstants& source, MaterialField field)
{
    if (IsColor(field))
    {
        SetColor(destination, field, GetColor(source, field));
    }
    else
    {
        SetFloat(destination, field, GetFloat(source, field));
    }
}

nlohmann::json MaterialFields::ToJson(const MaterialConstants& constants, MaterialField field)
{
    if (IsColor(field))
    {
        const DirectX::XMFLOAT3 color = GetColor(constants, field);
        return nlohmann::json::array({ ToShortestDouble(color.x), ToShortestDouble(color.y), ToShortestDouble(color.z) });
    }
    return ToShortestDouble(GetFloat(constants, field));
}

bool MaterialFields::FromJson(const nlohmann::json& json, MaterialConstants& constants, MaterialField field)
{
    if (IsColor(field))
    {
        if (!json.is_array() || json.size() != 3 || !json[0].is_number() || !json[1].is_number() || !json[2].is_number())
        {
            return false;
        }
        SetColor(constants, field, { json[0].get<float>(), json[1].get<float>(), json[2].get<float>() });
        return true;
    }
    if (!json.is_number())
    {
        return false;
    }
    SetFloat(constants, field, json.get<float>());
    return true;
}

void MaterialOverride::Set(MaterialField field, const MaterialConstants& source)
{
    mask |= 1u << static_cast<uint32_t>(field);
    MaterialFields::Copy(values, source, field);
}

void MaterialOverride::ApplyTo(Material& material) const
{
    for (int fieldIndex = 0; fieldIndex < MaterialFields::kCount; ++fieldIndex)
    {
        const auto field = static_cast<MaterialField>(fieldIndex);
        MaterialFields::Copy(material.Constants, Has(field) ? values : material.ImportedConstants, field);
    }
}
