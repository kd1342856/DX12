#pragma once
#include "../GPUResource.h"

//=============================
//
// 汎用のテクスチャ
//
//   2D・2D配列・キューブ・3Dと、ミップ、UAV(コンピュートからの書き込み)、RTVを1つのクラスで扱う。
//   SRVとUAVはシェーダーから見えるヒープに確保するので、番号をそのままバインドレスのシェーダー
//   (ResourceDescriptorHeap[番号])へ渡せる。Resize()で作り直しても番号は変わらない。
//
//=============================
enum class TextureDimension
{
    Texture2D,
    Texture2DArray,
    TextureCube,
    Texture3D,
};

struct GPUTextureDesc
{
    TextureDimension dimension = TextureDimension::Texture2D;
    UINT width = 1;
    UINT height = 1;
    // 2D配列は枚数、キューブは個数(面の数はこの6倍)、3Dは奥行き。2Dは1
    UINT depthOrArraySize = 1;
    UINT mipLevels = 1;
    DXGI_FORMAT format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    bool allowUAV = false;
    // 2Dのミップ0にだけRTVを作る
    bool allowRenderTarget = false;
    // ミップ1枚だけを見るSRVも作る(コンピュートで1つ上のミップから次のミップを作るときに読む)。
    // 2Dは Texture2D、配列とキューブは面を並べた Texture2DArray として見る
    bool createMipSRVs = false;
    // デバッグ用の名前(PIXなどに出る)
    const wchar_t* pName = nullptr;
};

class GPUTexture : public GPUResource
{
public:
    // 作ったときと、Resize()の後の状態。シェーダーから読める状態にしておく
    static constexpr D3D12_RESOURCE_STATES kInitialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

    bool Create(const GPUTextureDesc& desc);
    // 幅と高さだけを変えて作り直す。ディスクリプタの番号は変わらない。GPUの処理が終わってから呼ぶこと
    bool Resize(UINT width, UINT height);

    const GPUTextureDesc& GetDesc() const { return m_desc; }
    UINT GetWidth() const { return m_desc.width; }
    UINT GetHeight() const { return m_desc.height; }

    int GetSRVIndex() const { return m_srvIndex; }
    // ミップごとのUAV。allowUAVがfalseなら-1
    int GetUAVIndex(UINT mip = 0) const;
    // ミップ1枚だけのSRV。createMipSRVsがfalseなら-1
    int GetMipSRVIndex(UINT mip) const;
    // サブリソースの番号(ミップ、配列の要素(キューブは面の通し番号))。サブリソースごとの状態遷移に使う
    UINT GetSubresourceIndex(UINT mip, UINT arraySlice) const { return mip + arraySlice * m_desc.mipLevels; }
    // 配列の要素の数(キューブは面の数)
    UINT GetArraySize() const;
    int GetRTVIndex() const { return m_rtvIndex; }

    // RTVをクリアする(呼び出し側でRENDER_TARGETの状態にしておく)
    void ClearRenderTarget(float r, float g, float b, float a);

private:
    bool CreateResource();
    void WriteDescriptors();

    GPUTextureDesc m_desc;
    int m_srvIndex = -1;
    std::vector<int> m_uavIndices;
    std::vector<int> m_mipSrvIndices;
    int m_rtvIndex = -1;
};
