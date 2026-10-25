#pragma once
#include <mutex>
#include <vector>
#include <variant>
#include <functional>
#include <cstdint>
#include <d3d12.h>

// AssetHandle
#include "../../Framework/Manager/Asset/AssetHandle.h"
// AssetState
#include "../../Framework/Manager/Asset/AssetState.h"
// Material
#include "../../Graphics/GPUResource/Material/Material.h"
// MeshVertex / MeshFace
#include "../../Graphics/Geometry/Mesh/MeshData/MeshData.h"

class Texture;
class Mesh;

// ============================================================
// TextureUploadData
// ============================================================
struct TextureUploadData
{
    // ミップ1段分の、pixelsの中での位置と大きさ
    struct MipLevel
    {
        size_t offset     = 0;
        UINT   width      = 0;
        UINT   height     = 0;
        UINT   rowPitch   = 0;
        UINT   slicePitch = 0;
    };

    // 全ミップの画素を0段目から順に詰めたもの
    std::vector<uint8_t> pixels;
    std::vector<MipLevel> mipLevels;
    // 0段目の大きさ
    UINT        width     = 0;
    UINT        height    = 0;
    UINT        rowPitch  = 0;
    DXGI_FORMAT format    = DXGI_FORMAT_UNKNOWN;

    AssetHandle<Texture> targetHandle;
};

// ============================================================
// MeshUploadData
// ============================================================
struct MeshUploadData
{
    std::vector<MeshVertex> vertices;
    std::vector<MeshFace>   faces;
    Material                material;

    AssetHandle<Mesh> targetHandle;
};

// UploadData
using UploadData = std::variant<
    TextureUploadData,
    MeshUploadData,
    std::function<void()>
>;

struct UploadRequest
{
    UploadData data;
};

// ============================================================
// GPUUploadQueue
// ============================================================
class GPUUploadQueue
{
public:
    static GPUUploadQueue& Instance();

    void Submit(UploadRequest request);
    void Process();
    void Clear();

private:
    std::mutex m_mutex;
    std::vector<UploadRequest> m_queue;
};
