#pragma once
#include <string>
#include <wrl.h>
#include <d3d12.h>

//=============================
//
// バインドレスのコンピュートシェーダー(シェーダーモデル6.6)
//
//   ルートシグネチャは全てのコンピュートシェーダーで共通にする。
//     [0] 32ビットの定数 × kMaxRootConstants (b0): テクスチャのディスクリプタの番号などの小さな値
//     [1] 定数バッファ (b1): 行列などの大きな値(フレームの定数バッファから確保する)
//   テクスチャとバッファは、シェーダーの中で ResourceDescriptorHeap[番号] として直接参照する。
//   静的サンプラー s0: ポイント・クランプ、s1: リニア・クランプ
//
//   シェーダーモデル6.6(とリソースバインディングのTier 3)に対応していないGPUでは
//   IsSupported()がfalseになり、これを使う機能(TAAなど)は無効になる。
//
//=============================
class ComputeShader
{
public:
    static constexpr UINT kMaxRootConstants = 16;

    // 共通のルートシグネチャを作り、GPUが対応しているかを調べる。デバイスを作った後に一度だけ呼ぶ
    static void InitializeShared(ID3D12Device* pDevice);
    static void ReleaseShared();
    static bool IsSupported() { return s_isSupported; }

    // cs_6_6でコンパイルしてPSOを作る。失敗したらfalse
    bool Create(const std::wstring& filePath, const std::wstring& entryPoint = L"main");
    bool IsValid() const { return m_cpPipelineState != nullptr; }

    // PSOと共通のルートシグネチャを設定する。この後にSetConstants・Dispatchを呼ぶ
    void Begin() const;

    // b0の32ビットの定数(4バイト単位の構造体)
    template<typename T>
    void SetConstants(const T& constants) const
    {
        static_assert(sizeof(T) % 4 == 0, "定数は4バイト単位の大きさにする");
        static_assert(sizeof(T) / 4 <= kMaxRootConstants, "定数が多すぎる");
        SetConstantsRaw(&constants, static_cast<UINT>(sizeof(T) / 4));
    }

    // b1の定数バッファ
    template<typename T>
    void SetConstantBuffer(const T& data) const
    {
        SetConstantBufferAddress(GraphicsDevice::Instance().GetFrameConstantBufferAllocator()->AllocateData(data));
    }

    void Dispatch(UINT groupCountX, UINT groupCountY, UINT groupCountZ = 1) const;

    // 要素の数をスレッドグループの大きさで割り、切り上げる
    static UINT DivideRoundUp(UINT count, UINT groupSize) { return (count + groupSize - 1) / groupSize; }

private:
    void SetConstantsRaw(const void* pData, UINT count) const;
    void SetConstantBufferAddress(D3D12_GPU_VIRTUAL_ADDRESS address) const;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_cpPipelineState;

    static inline Microsoft::WRL::ComPtr<ID3D12RootSignature> s_cpRootSignature;
    static inline bool s_isSupported = false;
};
