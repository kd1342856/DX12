#pragma once
#include "../Shader/ComputeShader/ComputeShader.h"

class RenderTarget;
struct PostProcessSettings;

//=============================
//
// 自動露出
//
//   シーンの輝度のヒストグラムをGPUで作り、数フレーム遅れでCPUへ読み戻して、
//   暗い側と明るい側を除いた平均の明るさからEV100を決める。目のように少しずつ順応させる。
//   決めたEV100は次のフレームの前露出(ShaderManager::GetExposure)に使う。
//
//=============================
class AutoExposure
{
public:
    static AutoExposure& Instance()
    {
        static AutoExposure instance;
        return instance;
    }

    static constexpr UINT kHistogramBinCount = 64;
    // ヒストグラムで数える輝度の範囲(log2 nit)。2^-14 nit(EV100で-11)～2^6 nit(EV100で9)
    static constexpr float kMinLog2Luminance = -14.0f;
    static constexpr float kMaxLog2Luminance = 6.0f;

    void Initialize();

    // シーンの色からヒストグラムを作り、読み戻しを積む。exposureはこのフレームで使った露出(前露出)
    void Dispatch(RenderTarget* pSceneRT, float exposure);
    // 読み戻せたヒストグラムから目標のEV100を決め、順応させる。Dispatchの後に呼ぶ
    void Update(float deltaTime, const PostProcessSettings& settings, float manualEV100);

    // 自動露出が有効で、EV100が決まっているか
    bool IsActive() const { return m_isActive; }
    float GetEV100() const { return m_ev100; }
    // 平均の明るさから求めた、順応する前の目標のEV100(エディターの表示用)
    float GetTargetEV100() const { return m_targetEV100; }
    bool IsAvailable() const { return m_isInitialized; }

private:
    AutoExposure() = default;

    // CPUへの読み戻しは、GPUが書き終わったものだけを読むためにkReadbackSlotCount個を順に使う
    // (フレームの数FrameManager::kFrameCountより1つ多く持ち、一番古いものを読む)
    static constexpr int kReadbackSlotCount = 4;

    // ヒストグラムから、暗い側と明るい側を除いた平均のlog2輝度を求める。数えたものが無ければfalse
    bool ComputeAverageLog2Luminance(const uint32_t* pBins, float lowPercent, float highPercent, float& outAverage) const;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_cpHistogramBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_cpReadbackBuffers[kReadbackSlotCount];
    int m_histogramUAVIndex = -1;
    ComputeShader m_clearShader;
    ComputeShader m_buildShader;
    bool m_isInitialized = false;

    uint64_t m_dispatchCount = 0;
    bool m_isActive = false;
    float m_ev100 = 0.0f;
    float m_targetEV100 = 0.0f;
};
