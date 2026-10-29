#include "../../Pch.h"
#include "BufferViewer.h"
#include "Renderer.h"
#include "MotionVectors.h"
#include "TemporalAA.h"
#include "../GPUResource/RenderTarget/RenderTarget.h"
#include "../../Framework/DirectX/Utility/Profiler.h"

namespace
{
    // BufferView_CS.hlslのg_constants(b0)と同じ並び
    struct BufferViewConstants
    {
        uint32_t sourceIndex;
        uint32_t destinationIndex;
        uint32_t destinationWidth;
        uint32_t destinationHeight;
        uint32_t mode;
        uint32_t sourceWidth;
        uint32_t sourceHeight;
        uint32_t secondSourceIndex; // 2つのバッファを比べるときの、もう1つ(SRV)
    };

    // BufferView_CS.hlslのcbFrame(b1)と同じ並び
    struct BufferViewFrame
    {
        Math::Matrix inverseProjection;
    };

    constexpr UINT kThreadGroupSize = 8;

    // 変換する元のバッファ
    struct Source
    {
        ID3D12Resource* pResource = nullptr;
        int srvIndex = -1;
        int width = 0;
        int height = 0;
        bool isDepth = false;
        // 2つのバッファを比べるときの、もう1つ
        ID3D12Resource* pSecondResource = nullptr;
        int secondSrvIndex = -1;
    };

    Source FindSource(BufferViewer::Buffer buffer)
    {
        Source source;
        RenderTarget* pRT = nullptr;
        switch (buffer)
        {
            case BufferViewer::Buffer::SceneColor:
                pRT = Renderer::GetSceneHDRRenderTarget();
                break;
            case BufferViewer::Buffer::Depth:
                if (RenderTarget* pSceneRT = Renderer::GetSceneHDRRenderTarget())
                {
                    source.pResource = pSceneRT->GetDepthResource();
                    source.srvIndex = pSceneRT->GetDepthSRVIndex();
                    source.width = pSceneRT->GetWidth();
                    source.height = pSceneRT->GetHeight();
                    source.isDepth = true;
                }
                return source;
            case BufferViewer::Buffer::Normal:
            case BufferViewer::Buffer::Roughness:
                pRT = Renderer::GetNormalPrepassRenderTarget();
                break;
            case BufferViewer::Buffer::SSAO:
                // ぼかし終えた結果は0番に入っている
                pRT = Renderer::GetSSAORenderTarget(0);
                break;
            case BufferViewer::Buffer::Velocity:
            {
                auto& motionVectors = MotionVectors::Instance();
                if (motionVectors.IsValid())
                {
                    GPUTexture* pVelocity = motionVectors.GetVelocityTexture();
                    source.pResource = pVelocity->GetResource();
                    source.srvIndex = pVelocity->GetSRVIndex();
                    source.width = static_cast<int>(pVelocity->GetWidth());
                    source.height = static_cast<int>(pVelocity->GetHeight());
                }
                return source;
            }
            case BufferViewer::Buffer::Bloom:
                // ブルームのぼかしは、横と縦を1回ずつ通した結果が1番に入る
                pRT = Renderer::GetBloomBlurRenderTarget(1);
                break;
            case BufferViewer::Buffer::TAAResult:
            case BufferViewer::Buffer::TAAChange:
            {
                // TAAの履歴は、今のフレームの結果と1つ前のフレームの結果の2枚を交互に使う
                auto& temporalAA = TemporalAA::Instance();
                GPUTexture* pLatest = temporalAA.GetHistoryTexture(0);
                GPUTexture* pPrevious = temporalAA.GetHistoryTexture(1);
                if (!pLatest || (buffer == BufferViewer::Buffer::TAAChange && !pPrevious))
                {
                    return source;
                }
                source.pResource = pLatest->GetResource();
                source.srvIndex = pLatest->GetSRVIndex();
                source.width = static_cast<int>(pLatest->GetWidth());
                source.height = static_cast<int>(pLatest->GetHeight());
                if (buffer == BufferViewer::Buffer::TAAChange)
                {
                    source.pSecondResource = pPrevious->GetResource();
                    source.secondSrvIndex = pPrevious->GetSRVIndex();
                }
                return source;
            }
            default:
                break;
        }
        if (pRT)
        {
            source.pResource = pRT->GetResource();
            source.srvIndex = pRT->GetSRVIndex();
            source.width = pRT->GetWidth();
            source.height = pRT->GetHeight();
        }
        return source;
    }
}

const BufferViewer::BufferInfo& BufferViewer::GetInfo(Buffer buffer)
{
    static const BufferInfo kInfos[kBufferCount] =
    {
        { u8"シーンの色", u8"露出を掛けた後、トーンマッピングの前のシーンの明るさ(HDR)。\n明るい所は丸めて表示する(画面の見え方とは違う)。" },
        { u8"深度", u8"深度プリパスで描いたシーンの深度を、カメラからの距離にしたもの。\n近いほど白く、50 mより遠い所と空は黒。" },
        { u8"法線", u8"深度プリパスで描いた、カメラから見た面の向き(SSAOとSSRが使う)。\n赤・緑・青が横・縦・奥行きの向き。" },
        { u8"粗さ", u8"法線のバッファの4つ目の値に入れた、表面の粗さ(SSRが使う)。\n黒がつるつる、白がざらざら。" },
        { u8"SSAO", u8"画面の深度から求めた環境光の遮蔽(半分の解像度)。\n黒いほど、隅や隙間で光が届きにくい。SSAOを切っていると前の中身のまま。" },
        { u8"速度", u8"前のフレームから画面の上をどれだけ動いたか(TAAとモーションブラーが使う)。\n色が動いた向き、濃さが速さ(16ピクセルで一番濃い)。止まっている所は黒。" },
        { u8"ブルーム", u8"明るい所からにじむ光(4分の1の解像度)。明るい所は丸めて表示する。\nブルームを切っていると前の中身のまま。" },
        { u8"TAAの結果", u8"TAAで前のフレームまでの結果と混ぜた、今のフレームの色(次のフレームの履歴になる)。\n明るい所は丸めて表示する。TAAを切っていると前の中身のまま。" },
        { u8"前のフレームとの差", u8"TAAの結果を、1つ前のフレームの結果と比べた差(画面の色で、4倍に強調)。\n止まっているのに光る所は、TAAでもちらついている所。黒は変わらない所。TAAを切っていると前の中身のまま。" },
    };
    return kInfos[static_cast<int>(buffer)];
}

void BufferViewer::GetSourceSize(Buffer buffer, int& outWidth, int& outHeight) const
{
    const Source source = FindSource(buffer);
    outWidth = source.width;
    outHeight = source.height;
}

bool BufferViewer::EnsureResources()
{
    if (m_isCreated)
    {
        return true;
    }
    if (m_hasFailed || !ComputeShader::IsSupported())
    {
        return false;
    }
    // 失敗しても、シェーダーを直せばホットリロードで使えるようになるので、作り直しはしない
    m_shader.Create(L"Asset/Shader/Debug/BufferView_CS.hlsl");

    GPUTextureDesc desc;
    desc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.allowUAV = true;
    desc.width = kThumbnailWidth;
    desc.height = kThumbnailHeight;
    desc.pName = L"BufferViewThumbnail";
    auto& device = GraphicsDevice::Instance();
    for (int bufferIndex = 0; bufferIndex < kBufferCount; ++bufferIndex)
    {
        if (!m_thumbnails[bufferIndex].Create(desc))
        {
            m_hasFailed = true;
            return false;
        }
        // ImGuiのヒープの番号は返せないので、作るのは一度だけにする
        m_thumbnailImGuiSrvIndices[bufferIndex] = device.AllocateImGuiSRV(m_thumbnails[bufferIndex].GetResource());
    }
    desc.width = kLargeWidth;
    desc.height = kLargeHeight;
    desc.pName = L"BufferViewLarge";
    if (!m_large.Create(desc))
    {
        m_hasFailed = true;
        return false;
    }
    m_largeImGuiSrvIndex = device.AllocateImGuiSRV(m_large.GetResource());
    m_isCreated = true;
    return true;
}

void BufferViewer::Dispatch(const Math::Matrix& projection)
{
    if (!m_isRequested)
    {
        return;
    }
    m_isRequested = false;
    if (!EnsureResources() || !m_shader.IsValid())
    {
        return;
    }

    auto& device = GraphicsDevice::Instance();
    auto* pContext = device.GetContextManager()->GetGraphicsContext();
    PROFILE_GPU_SCOPE(device.GetCmdList(), "BufferView");

    // 元のバッファをコンピュートから読める状態に、書き込み先をUAVにまとめて変える
    Source sources[kBufferCount];
    for (int bufferIndex = 0; bufferIndex < kBufferCount; ++bufferIndex)
    {
        sources[bufferIndex] = FindSource(static_cast<Buffer>(bufferIndex));
        if (sources[bufferIndex].pResource)
        {
            pContext->TransitionResource(sources[bufferIndex].pResource, GPUTexture::kInitialState);
        }
        if (sources[bufferIndex].pSecondResource)
        {
            pContext->TransitionResource(sources[bufferIndex].pSecondResource, GPUTexture::kInitialState);
        }
        pContext->TransitionResource(m_thumbnails[bufferIndex].GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    }
    pContext->TransitionResource(m_large.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    pContext->FlushResourceBarriers();

    m_shader.Begin();
    BufferViewFrame frame = {};
    frame.inverseProjection = projection.Invert();
    m_shader.SetConstantBuffer(frame);
    for (int bufferIndex = 0; bufferIndex < kBufferCount; ++bufferIndex)
    {
        Convert(static_cast<Buffer>(bufferIndex), m_thumbnails[bufferIndex]);
    }
    Convert(m_selected, m_large);

    // ImGuiが読めるように戻す。深度は次のフレームの描画のために書き込みへ戻す
    for (int bufferIndex = 0; bufferIndex < kBufferCount; ++bufferIndex)
    {
        pContext->TransitionResource(m_thumbnails[bufferIndex].GetResource(), GPUTexture::kInitialState);
        if (sources[bufferIndex].pResource && sources[bufferIndex].isDepth)
        {
            pContext->TransitionResource(sources[bufferIndex].pResource, D3D12_RESOURCE_STATE_DEPTH_WRITE);
        }
    }
    pContext->TransitionResource(m_large.GetResource(), GPUTexture::kInitialState);
    pContext->FlushResourceBarriers();
    m_hasContent = true;
}

void BufferViewer::Convert(Buffer buffer, GPUTexture& destination)
{
    const Source source = FindSource(buffer);
    if (!source.pResource || source.srvIndex < 0)
    {
        return;
    }
    BufferViewConstants constants = {};
    constants.sourceIndex = static_cast<uint32_t>(source.srvIndex);
    constants.destinationIndex = static_cast<uint32_t>(destination.GetUAVIndex());
    constants.destinationWidth = destination.GetWidth();
    constants.destinationHeight = destination.GetHeight();
    constants.mode = static_cast<uint32_t>(buffer);
    constants.sourceWidth = static_cast<uint32_t>(source.width);
    constants.sourceHeight = static_cast<uint32_t>(source.height);
    constants.secondSourceIndex = static_cast<uint32_t>((source.secondSrvIndex >= 0) ? source.secondSrvIndex : source.srvIndex);
    m_shader.SetConstants(constants);
    m_shader.Dispatch(ComputeShader::DivideRoundUp(constants.destinationWidth, kThreadGroupSize), ComputeShader::DivideRoundUp(constants.destinationHeight, kThreadGroupSize));
}
