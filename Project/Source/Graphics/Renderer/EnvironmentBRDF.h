#pragma once
#include "../GPUResource/GPUTexture/GPUTexture.h"
#include "../Shader/ComputeShader/ComputeShader.h"

//=============================
//
// 環境BRDFのLUT(スプリットサム近似)
//
//   NdotVとラフネスごとに、鏡面反射を環境光で照らしたときの量を F0 × A + B の(A, B)として持つ。
//   反射プローブの鏡面と、マルチスキャッタのエネルギー補償(1回の反射で失われる分を足し戻す)に使う。
//   起動して最初のフレームで一度だけコンピュートで作る。シェーダーからはt21で読む(inc_Lighting.hlsli)。
//
//=============================
class EnvironmentBRDF
{
public:
    static EnvironmentBRDF& Instance()
    {
        static EnvironmentBRDF instance;
        return instance;
    }

    static constexpr UINT kSize = 128;
    static constexpr UINT kSampleCount = 1024;
    // シェーダーのレジスタ(t21)
    static constexpr uint32_t kShaderRegister = 21;

    void Initialize();
    // まだ作っていなければ作る。フレームの描画を始めるときに呼ぶ(コマンドリストに積む)
    void GenerateIfNeeded();

    // 作り終えていればSRVの番号、まだなら-1(シェーダーは解析的な近似に切り替える)
    int GetSRVIndex() const { return m_isGenerated ? m_texture.GetSRVIndex() : -1; }

private:
    EnvironmentBRDF() = default;

    GPUTexture m_texture;
    ComputeShader m_shader;
    bool m_isInitialized = false;
    bool m_isGenerated = false;
};
