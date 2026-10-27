#pragma once
#include <d3d12.h>
#include <wrl.h>

class RenderTarget;

//=============================
//
// ピクセルインスペクター
//
//   シーンのHDRのRT(露出を掛けた後、トーンマッピングの前)から1画素だけをCPUへ読み戻す。
//   GPUが書き終えたものだけを読むよう、自動露出と同じく数フレーム遅れの読み戻しを使う。
//
//=============================
class PixelProbe
{
public:
    static PixelProbe& Instance()
    {
        static PixelProbe instance;
        return instance;
    }

    struct Result
    {
        bool isValid = false;
        int x = 0;
        int y = 0;
        float red = 0.0f;
        float green = 0.0f;
        float blue = 0.0f;
        float exposure = 1.0f; // 読んだフレームの露出(前露出を割り戻して絶対の輝度にするため)
    };

    // 調べる場所(画面の左上を0、右下を1とした割合)。毎フレーム呼ぶ。呼ばなければ読み戻さない
    void Request(float u, float v);
    // シーンのRTから1画素を読み戻しのバッファへ写す。exposureはこのフレームの露出
    void Dispatch(RenderTarget* pSceneRT, float exposure);
    // GPUが書き終えた一番新しい結果
    const Result& GetResult() const { return m_result; }

private:
    PixelProbe() = default;
    bool EnsureBuffers();
    void ReadOldest();

    static constexpr int kSlotCount = 4;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_cpReadbackBuffers[kSlotCount];
    Result m_slotRequests[kSlotCount] = {};
    DXGI_FORMAT m_slotFormats[kSlotCount] = {};
    uint64_t m_dispatchCount = 0;

    bool m_isRequested = false;
    float m_requestU = 0.0f;
    float m_requestV = 0.0f;
    Result m_result;
};
