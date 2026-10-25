#include "../../../Pch.h"
#include "../../../Framework/DirectX/Utility/Profiler.h"
#include "../../GDF/GDF.h"
#include "ParticleShader.h"

void ParticleShader::Create(GraphicsDevice* pGraphicsDevice)
{
    m_pDevice = pGraphicsDevice;

    m_pProgram = ShaderManager::Instance().LoadShader(L"Asset/Shader/ParticleShader/ParticleShader_VS.hlsl", L"Asset/Shader/ParticleShader/ParticleShader_PS.hlsl");

    PipelineDesc desc;
    desc.InputLayouts = {}; // 頂点バッファなし(SV_VertexIDから四角形を作る)
    desc.Formats = {DXGI_FORMAT_R16G16B16A16_FLOAT};
    desc.BlendMode = BlendMode::Add;
    desc.CullMode = CullMode::None;
    // 壁や物の後ろには隠れるが、粒同士や半透明物の奥行きは書き込まない
    desc.IsDepth = true;
    desc.IsDepthMask = false;
    desc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    m_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    m_pPipelineState = ShaderManager::Instance().GetPipelineState(m_pProgram, desc);
}

void ParticleShader::Draw(RenderContext& context, const std::vector<Instance>& instances)
{
    if (!m_pProgram || !m_pPipelineState || instances.empty())
    {
        return;
    }

    Begin(context);

    int b0 = GetRootParameterIndex(ShaderBindingType::CBV, 0);
    if (b0 != -1)
    {
        context.BindCamera(b0);
    }
    int b2 = GetRootParameterIndex(ShaderBindingType::CBV, 2);
    if (b2 == -1)
    {
        return;
    }

    struct ParticleCBuffer
    {
        Instance Particles[kMaxPerBatch];
    };
    for (size_t start = 0; start < instances.size(); start += kMaxPerBatch)
    {
        size_t count = (std::min)(instances.size() - start, (size_t)kMaxPerBatch);
        ParticleCBuffer cb = {};
        memcpy(cb.Particles, &instances[start], count * sizeof(Instance));
        GDF::Instance().BindCBuffer(b2, cb);

        Profiler::Instance().AddDrawCall("ParticleShader", 1);
        m_pDevice->GetCmdList()->DrawInstanced((UINT)count * 6, 1, 0, 0);
    }
}
