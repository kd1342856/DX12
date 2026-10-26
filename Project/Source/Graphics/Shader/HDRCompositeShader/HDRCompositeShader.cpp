#include "../../../Pch.h"
#include "HDRCompositeShader.h"
#include "../../GPUResource/RenderTarget/RenderTarget.h"

namespace
{
    // HDRComposite_PS.hlslのcbHDRComposite(b0)と同じ並び
    struct HDRCompositeConstants
    {
        float paperWhiteNits;
        float pad[3];
    };
}

void HDRCompositeShader::Create(GraphicsDevice* pGraphicsDevice)
{
    m_pDevice = pGraphicsDevice;
    m_pProgram = ShaderManager::Instance().LoadShader(L"Asset/Shader/PostProcessShader/PostProcessShader_VS.hlsl", L"Asset/Shader/PostProcessShader/HDRComposite_PS.hlsl");

    PipelineDesc desc;
    desc.InputLayouts = {};
    desc.Formats = { DXGI_FORMAT_R10G10B10A2_UNORM };
    desc.IsDepth = false;
    m_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    m_pPipelineState = ShaderManager::Instance().GetPipelineState(m_pProgram, desc);
}

void HDRCompositeShader::Draw(RenderTarget* pSceneRT, RenderTarget* pUIRT, float paperWhiteNits)
{
    auto* pCmdList = m_pDevice->GetCmdList();
    // ImGuiが自分のヒープを束縛した後に呼ばれるので、いつものヒープに戻す
    ID3D12DescriptorHeap* pHeaps[] = { m_pDevice->GetDescriptorHeapManager()->GetCBVSRVUAVAllocator()->GetHeap() };
    pCmdList->SetDescriptorHeaps(1, pHeaps);

    RenderContext context;
    Begin(context);

    HDRCompositeConstants constants = {};
    constants.paperWhiteNits = paperWhiteNits;
    const int b0 = GetRootParameterIndex(ShaderBindingType::CBV, 0);
    if (b0 != -1)
    {
        GDF::Instance().BindCBuffer(b0, constants);
    }
    auto* pAllocator = m_pDevice->GetDescriptorHeapManager()->GetCBVSRVUAVAllocator();
    const int t0 = GetRootParameterIndex(ShaderBindingType::SRV, 0);
    if (t0 != -1)
    {
        pCmdList->SetGraphicsRootDescriptorTable(t0, pAllocator->GetGPUHandle(pSceneRT->GetSRVIndex()));
    }
    const int t1 = GetRootParameterIndex(ShaderBindingType::SRV, 1);
    if (t1 != -1)
    {
        pCmdList->SetGraphicsRootDescriptorTable(t1, pAllocator->GetGPUHandle(pUIRT->GetSRVIndex()));
    }
    pCmdList->DrawInstanced(3, 1, 0, 0);
}
