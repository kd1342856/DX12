#pragma once
#include "../GPUResource/GPUTexture/GPUTexture.h"
#include "../Shader/ComputeShader/ComputeShader.h"
#include "../Shadow/LocalShadowManager.h"

//=============================
//
// 影の中身の可視化(シェーダーエディターの「影の中身」)
//
//   月の影・スポットの影(2D配列)・点光源の影(キューブ配列)の深度を、近いほど白い灰色にした
//   小さなテクスチャへコンピュートで書き(ShadowView_CS.hlsl)、ImGuiで並べる。
//   パネルが毎フレームRequest()を呼んでいる間だけ動く。
//
//=============================
class ShadowViewer
{
public:
    static ShadowViewer& Instance()
    {
        static ShadowViewer instance;
        return instance;
    }

    static constexpr int kSpotCount = static_cast<int>(LocalShadowManager::kSpotShadowCount);
    static constexpr int kCubeCount = static_cast<int>(LocalShadowManager::kCubeShadowCount);
    static constexpr UINT kDirectionalSize = 256;
    static constexpr UINT kSpotSize = 192;
    // キューブは6面を3×2に並べる
    static constexpr UINT kCubeFaceSize = 96;

    // 見たいときに毎フレーム呼ぶ。呼ばなければ何もしない
    void Request() { m_isRequested = true; }
    // ポストの後に呼ぶ
    void Dispatch();

    bool IsAvailable() const { return ComputeShader::IsSupported() && !m_hasFailed; }
    bool HasContent() const { return m_hasContent; }

    int GetDirectionalImGuiSRVIndex() const { return m_directional.imGuiSrvIndex; }
    int GetSpotImGuiSRVIndex(int slot) const { return m_spots[slot].imGuiSrvIndex; }
    int GetCubeImGuiSRVIndex(int slot) const { return m_cubes[slot].imGuiSrvIndex; }

private:
    ShadowViewer() = default;
    bool EnsureResources();

    struct Thumbnail
    {
        GPUTexture texture;
        int imGuiSrvIndex = -1;
    };
    bool CreateThumbnail(Thumbnail& thumbnail, UINT width, UINT height, const wchar_t* pName);
    void Convert(Thumbnail& thumbnail, int sourceIndex, uint32_t mode, uint32_t layer, float nearZ, float farZ);

    ComputeShader m_shader;
    Thumbnail m_directional;
    Thumbnail m_spots[kSpotCount];
    Thumbnail m_cubes[kCubeCount];
    bool m_isCreated = false;
    bool m_hasFailed = false;
    bool m_hasContent = false;
    bool m_isRequested = false;
};
