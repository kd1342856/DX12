#pragma once
#include "../GPUResource/GPUTexture/GPUTexture.h"
#include "../GPUResource/CBufferData/CBufferData.h"
#include "../Shader/ComputeShader/ComputeShader.h"

class RenderTarget;

//=============================
//
// 反射プローブ
//
//   部屋(RoomArea)ごとに1つ、部屋の中心からシーンをキューブマップに撮り、ラフネスごとにぼかしたミップを作る。
//   鏡面の環境光は、そのピクセルが入っている部屋の箱で反射の向きを補正して(ボックス投影)このキューブから読む。
//   撮影は1フレームに1面ずつ、全部のプローブを順に回り続ける(照明の点灯・消灯や扉の開閉にも追従する)。
//   撮った値は前露出なので、撮ったときの露出を覚えておき、読むときに今の露出との比を掛ける。
//
//=============================
class ReflectionProbes
{
public:
    static ReflectionProbes& Instance()
    {
        static ReflectionProbes instance;
        return instance;
    }

    static constexpr int kMaxProbes = 16;
    static constexpr UINT kCaptureSize = 128;
    // 128, 64, 32, 16, 8, 4。最後のミップがラフネス1
    static constexpr UINT kMipCount = 6;
    // シェーダーのレジスタ(t22)
    static constexpr uint32_t kShaderRegister = 22;

    struct Volume
    {
        Math::Vector3 min;
        Math::Vector3 max;
    };

    void Initialize();

    // 部屋の箱を渡す(毎フレーム)。並びや大きさが変わったら全部撮り直す
    void SetVolumes(const std::vector<Volume>& volumes);

    // このフレームで撮る面があれば、その視点と、描き込むRTを返す。描いた後にEndFaceCaptureを呼ぶ
    bool BeginFaceCapture(Math::Matrix& outView, Math::Matrix& outProjection, RenderTarget*& outTarget);
    // 描いた面をキューブへ写す。6面がそろったらミップを作ってぼかし、そのプローブを使えるようにする。
    // exposureは撮ったときの露出(前露出)
    void EndFaceCapture(float exposure);

    // ライトの定数バッファに、プローブの箱・撮った位置・露出の比を書く
    void FillShaderData(CBufferData::Light& lightData, float exposure) const;

    // まだ使えなければ-1
    int GetSRVIndex() const { return m_isInitialized ? m_probeTexture.GetSRVIndex() : -1; }
    bool IsAvailable() const { return m_isInitialized; }

private:
    ReflectionProbes() = default;

    void FilterProbe(int probeIndex);
    static Math::Vector3 GetCapturePosition(const Volume& volume);

    struct Probe
    {
        Volume volume;
        float captureExposure = 0.0f; // 0なら、まだ6面を撮り終えていない
    };

    GPUTexture m_probeTexture;   // 全プローブのキューブ配列(ミップ付き)
    GPUTexture m_stagingTexture; // 撮影中の1つのキューブ(ミップ付き)
    std::unique_ptr<RenderTarget> m_upCaptureTarget;
    ComputeShader m_downsampleShader;
    ComputeShader m_filterShader;
    bool m_isInitialized = false;

    std::vector<Probe> m_probes;
    int m_captureProbe = 0;
    int m_captureFace = 0;
};
