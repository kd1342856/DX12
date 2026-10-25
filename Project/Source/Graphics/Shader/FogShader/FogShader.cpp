#include "../../../Pch.h"
#include "../../../Framework/DirectX/Utility/Profiler.h"
#include "FogShader.h"

void FogShader::Create(GraphicsDevice* pGraphicsDevice)
{
    m_pDevice = pGraphicsDevice;

    m_pProgram = ShaderManager::Instance().LoadShader(L"Asset/Shader/FogShader/FogShader_VS.hlsl", L"Asset/Shader/FogShader/FogShader_PS.hlsl");

    PipelineDesc desc;
    desc.InputLayouts = {};
    desc.Formats = { DXGI_FORMAT_R8G8B8A8_UNORM };
    desc.BlendMode = BlendMode::None; 
    desc.CullMode = CullMode::None;
    desc.IsDepth = false; 

    m_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    m_pPipelineState = ShaderManager::Instance().GetPipelineState(m_pProgram, desc);
}

void FogShader::Draw(float time)
{
    if (!m_pProgram || !m_pPipelineState) {
        GraphicsDevice::Instance().ClearBackBuffer(0.0f, 0.0f, 1.0f, 1.0f); // BLUE means shader failed to load!
        return;
    }

    RenderContext context;
    Begin(context);

    struct FogCBuffer {
        float time;
        float pad0, pad1, pad2;
    };
    
    FogCBuffer cb;
    cb.time = time;
    cb.pad0 = cb.pad1 = cb.pad2 = 0.0f;

    int b0 = GetRootParameterIndex(ShaderBindingType::CBV, 0);
    if (b0 != -1) {
        GDF::Instance().BindCBuffer(b0, cb);
    }

    Profiler::Instance().AddDrawCall("FogShader", 1);
    m_pDevice->GetCmdList()->DrawInstanced(3, 1, 0, 0);
}
