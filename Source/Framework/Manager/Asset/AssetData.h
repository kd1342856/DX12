#pragma once
#include <vector>
#include <string>
#include <DirectXCollision.h>
#include "AssetHandle.h"
#include "../../../Graphics/Geometry/Mesh/MeshData/MeshData.h"

class Texture;
class Material;

#include "../../../Framework/ECS/Components/Data/AnimationData.h"

struct AssetMeshData
{
    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;
    DirectX::BoundingBox bounds;
    AssetHandle<Material> materialHandle;
};

struct MaterialTexture
{
    AssetHandle<Texture> handle;
    bool valid = false;
    uint32_t uvSet = 0;
    float scale = 1.0f;
    float strength = 1.0f;
};

struct AssetMaterialData
{
    std::string name;
    
    MaterialTexture baseColor;
    MaterialTexture normal;
    MaterialTexture metallicRoughness;
    MaterialTexture occlusion;
    MaterialTexture emissive;

    Math::Vector4 baseColorFactor = { 1.0f, 1.0f, 1.0f, 1.0f };
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;
    float normalScale = 1.0f;
    float occlusionStrength = 1.0f;
    float emissiveStrength = 1.0f;
    float emissiveFactorX = 0.0f;
    float emissiveFactorY = 0.0f;
    float emissiveFactorZ = 0.0f;

    int alphaMode = 0; // 0:Opaque, 1:Mask, 2:Blend
    int proceduralType = 0; // 0:None, 1:Blood, 2:Cobweb
};

struct AssetAnimationData
{
    std::string name;
    std::vector<AnimationChannel> channels;
    float duration = 0.0f;
};

// Texture decode data to be passed to main thread
struct AssetTextureData
{
    int width = 0;
    int height = 0;
    int channels = 0;
    std::vector<uint8_t> pixels;
    std::string filepath; // for debugging/cache key
};
