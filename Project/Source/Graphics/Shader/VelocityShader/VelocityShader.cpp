#include "../../../Pch.h"
#include "VelocityShader.h"
#include "../../GDF/GDF.h"
#include "../../Renderer/MotionVectors.h"
#include "../../../Framework/Manager/Asset/TextureManager.h"

void VelocityShader::Create(GraphicsDevice* pGraphicsDevice)
{
    m_pDevice = pGraphicsDevice;
    m_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    PipelineDesc desc;
    desc.Formats = { MotionVectors::kFormat };
    // ÉÅÉCÉìÇÃï`âÊ(LitShaderÅESkinningShader)Ç∆ìØÇ∂Ç≠óºñ Çï`Ç≠
    desc.CullMode = CullMode::None;
    desc.BlendMode = BlendMode::None;
    // ÉÅÉCÉìÇÃï`âÊÇ≈èëÇ¢ÇΩê[ìxÇ∆ìØÇ∂ê[Ç≥ÇÃÇ∆Ç±ÇÎÇæÇØè„èëÇ´Ç∑ÇÈ
    desc.IsDepthMask = false;
    desc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    m_pStaticProgram = ShaderManager::Instance().LoadShader(L"Asset/Shader/VelocityShader/Velocity_VS.hlsl", L"Asset/Shader/VelocityShader/Velocity_PS.hlsl");
    desc.InputLayouts = { InputLayout::POSITION, InputLayout::TEXCOORD, InputLayout::NORMAL, InputLayout::COLOR, InputLayout::TANGENT };
    m_pStaticPipelineState = ShaderManager::Instance().GetPipelineState(m_pStaticProgram, desc);

    m_pSkinnedProgram = ShaderManager::Instance().LoadShader(L"Asset/Shader/VelocityShader/VelocitySkinned_VS.hlsl", L"Asset/Shader/VelocityShader/Velocity_PS.hlsl");
    desc.InputLayouts = {
        InputLayout::POSITION, InputLayout::TEXCOORD, InputLayout::NORMAL,
        InputLayout::COLOR, InputLayout::TANGENT,
        InputLayout::SKININDEX, InputLayout::SKINWEIGHT
    };
    m_pSkinnedPipelineState = ShaderManager::Instance().GetPipelineState(m_pSkinnedProgram, desc);

    m_pProgram = m_pStaticProgram;
    m_pPipelineState = m_pStaticPipelineState;
}

void VelocityShader::BeginStatic(RenderContext& context, const FrameConstants& frame)
{
    BeginProgram(context, m_pStaticProgram, m_pStaticPipelineState, frame);
}

void VelocityShader::BeginSkinned(RenderContext& context, const FrameConstants& frame)
{
    BeginProgram(context, m_pSkinnedProgram, m_pSkinnedPipelineState, frame);
}

void VelocityShader::BeginProgram(RenderContext& context, ShaderProgram* pProgram, ID3D12PipelineState* pPipelineState, const FrameConstants& frame)
{
    m_pProgram = pProgram;
    m_pPipelineState = pPipelineState;
    GraphicsShader::Begin(context);

    const int b0 = GetRootParameterIndex(ShaderBindingType::CBV, 0);
    if (b0 != -1)
    {
        context.BindCamera(b0);
    }
    const int b4 = GetRootParameterIndex(ShaderBindingType::CBV, 4);
    if (b4 != -1)
    {
        GDF::Instance().BindCBuffer(b4, frame);
    }
}

void VelocityShader::SetBones(const std::vector<Math::Matrix>& bones, const std::vector<Math::Matrix>& previousBones)
{
    CBufferData::Bones currentData = {};
    CBufferData::Bones previousData = {};
    const size_t count = (std::min)(bones.size(), static_cast<size_t>(256));
    for (size_t i = 0; i < count; ++i)
    {
        currentData.mBones[i] = bones[i];
        previousData.mBones[i] = (i < previousBones.size()) ? previousBones[i] : bones[i];
    }
    const int b2 = GetRootParameterIndex(ShaderBindingType::CBV, 2);
    if (b2 != -1)
    {
        GDF::Instance().BindCBuffer(b2, currentData);
    }
    const int b5 = GetRootParameterIndex(ShaderBindingType::CBV, 5);
    if (b5 != -1)
    {
        GDF::Instance().BindCBuffer(b5, previousData);
    }
}

void VelocityShader::SetWorld(const Math::Matrix& world, const Math::Matrix& previousWorld)
{
    CBufferData::PerDraw currentData;
    currentData.mWorld = world;
    CBufferData::PerDraw previousData;
    previousData.mWorld = previousWorld;

    const int b1 = GetRootParameterIndex(ShaderBindingType::CBV, 1);
    if (b1 != -1)
    {
        GDF::Instance().BindCBuffer(b1, currentData);
    }
    const int b3 = GetRootParameterIndex(ShaderBindingType::CBV, 3);
    if (b3 != -1)
    {
        GDF::Instance().BindCBuffer(b3, previousData);
    }
}

void VelocityShader::BeforeDrawMesh(const Mesh& mesh, const Material& material)
{
    const int b6 = GetRootParameterIndex(ShaderBindingType::CBV, 6);
    if (b6 != -1)
    {
        GDF::Instance().BindCBuffer(b6, material.Constants);
    }
    const int t0 = GetRootParameterIndex(ShaderBindingType::SRV, 0);
    if (t0 != -1)
    {
        auto* pTexture = TextureManager::Instance().Get(material.BaseColor.handle);
        if (pTexture && material.BaseColor.valid && pTexture->IsReady())
        {
            pTexture->Set(t0);
        }
        else
        {
            GraphicsDevice::Instance().GetWhiteTex()->Set(t0);
        }
    }
}
