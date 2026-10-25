#pragma once
#include "../GPUResource/GPUTexture/GPUTexture.h"
#include "../Shader/ComputeShader/ComputeShader.h"

class RenderTarget;

//=============================
//
// TAA(テンポラル・アンチエイリアス)
//
//   ネイティブ解像度のまま、毎フレーム投影をピクセル以下ずらして(Halton列のジッター)描き、
//   速度(MotionVectors)でたどった前のフレームの結果と混ぜる。
//   後でDLAAやFSRに差し替えられるよう、入力(色・深度・速度・ジッター)と出力の形はそれらと合わせてある。
//
//=============================
class TemporalAA
{
public:
    static TemporalAA& Instance()
    {
        static TemporalAA instance;
        return instance;
    }

    void Initialize(int width, int height);
    void Resize(int width, int height);

    // そのフレームのジッターを決める。シーンを描く前に一度だけ呼ぶ。
    // 戻り値は投影行列に足すずれ(NDC)。TAAが無効なら0
    Math::Vector2 BeginFrame(bool isEnabled, int width, int height);
    // 投影行列にジッターを足す(透視投影でも平行投影でも、NDCで同じだけずれる)
    static Math::Matrix ApplyJitter(const Math::Matrix& projection, const Math::Vector2& jitterNdc);

    // pSceneRTの色を、履歴と混ぜた結果で置き換える。速度はMotionVectorsのものを使う
    void Resolve(RenderTarget* pSceneRT, float currentWeight);

    // 次のフレームで履歴を使わない(カメラの切り替え・瞬間移動・解像度の変更のとき)
    void InvalidateHistory() { m_isHistoryValid = false; }

    bool IsEnabled() const { return m_isEnabledThisFrame; }
    bool IsAvailable() const { return m_isInitialized; }

private:
    TemporalAA() = default;

    static float Halton(uint32_t index, uint32_t base);

    GPUTexture m_historyTextures[2];
    ComputeShader m_resolveShader;
    bool m_isInitialized = false;
    bool m_isEnabledThisFrame = false;
    bool m_isHistoryValid = false;
    uint32_t m_frameIndex = 0;
    // 今のフレームで書き込む履歴(0か1)
    int m_writeIndex = 0;
};
