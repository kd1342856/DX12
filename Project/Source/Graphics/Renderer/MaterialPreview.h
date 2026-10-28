#pragma once
#include <string>
#include "../GPUResource/GPUTexture/GPUTexture.h"
#include "../Shader/ComputeShader/ComputeShader.h"
#include "../GPUResource/Material/Material.h"

//=============================
//
// マテリアルのプレビュー(シェーダーエディターのマテリアルのパネルの球)
//
//   マテリアルの値とテクスチャで照らした球を、コンピュートで小さなテクスチャに描く(MaterialPreview_CS.hlsl)。
//   光は固定なので、シーンのライトや露出に左右されない。パネルが毎フレームRequest()を呼んでいる間だけ描く。
//   同時に開いたマテリアルの分だけ枠を使い、足りなければ最も長く頼まれていない枠を使い回す。
//
//=============================
class MaterialPreview
{
public:
    static MaterialPreview& Instance()
    {
        static MaterialPreview instance;
        return instance;
    }

    static constexpr UINT kSize = 160;
    static constexpr int kSlotCount = 8;

    // このフレームで見たいマテリアルを頼む(keyはマテリアルを見分ける名前)。
    // 前のフレームまでに描いた絵の、ImGuiのヒープのSRVの番号を返す。まだ無ければ-1
    int Request(const std::string& key, const Material& material);
    // 頼まれた分を描く。ポストの後に呼ぶ
    void Dispatch();

    // コンピュートが使えないGPUではfalse
    bool IsAvailable() const { return ComputeShader::IsSupported() && !m_hasFailed; }

private:
    MaterialPreview() = default;
    bool EnsureResources();

    struct Slot
    {
        std::string key;
        GPUTexture texture;
        int imGuiSrvIndex = -1;
        uint64_t lastRequestedFrame = 0;
        bool hasContent = false;
        bool isRequested = false;
        MaterialConstants constants;
        uint32_t textureIndices[5] = {}; // 色・法線・金属と粗さ・AO・発光
        float textureLod = 0.0f;
    };

    ComputeShader m_shader;
    Slot m_slots[kSlotCount];
    uint64_t m_frame = 1;
    bool m_isCreated = false;
    bool m_hasFailed = false;
};
