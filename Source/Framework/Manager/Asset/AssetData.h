#pragma once
#include <vector>
#include <string>
#include <DirectXCollision.h>
#include "AssetHandle.h"
#include "../../../../Graphics/Geometry/Mesh/MeshData/MeshData.h"

class Texture;

#include "../../../Framework/ECS/Components/Data/AnimationData.h"

struct AssetMeshData
{
    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;
    DirectX::BoundingBox bounds;
};

struct AssetMaterialData
{
    std::string name;
    AssetHandle<Texture> baseColor;
    AssetHandle<Texture> normal;
    AssetHandle<Texture> metallicRoughness;
    float metallic = 0.0f;
    float roughness = 1.0f;
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
