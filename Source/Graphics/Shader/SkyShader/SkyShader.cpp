#include "../../../Pch.h"
#include "SkyShader.h"

#include "../../GDF/GDF.h"
#include "../../Renderer/RenderContext.h"
#include "../../../Framework/Manager/Asset/TextureManager.h"

void SkyShader::Create(GraphicsDevice* pGraphicsDevice)
{
    m_pDevice = pGraphicsDevice;

    m_pProgram = ShaderManager::Instance().LoadShader(L"Asset/Shader/SkyShader/SkyShader_VS.hlsl", L"Asset/Shader/SkyShader/SkyShader_PS.hlsl");

    PipelineDesc desc;
    desc.InputLayouts = { InputLayout::POSITION, InputLayout::TEXCOORD, InputLayout::NORMAL, InputLayout::COLOR, InputLayout::TANGENT };
    desc.Formats = { DXGI_FORMAT_R16G16B16A16_FLOAT };
    
    // Sky sphere specific states: Depth Write OFF, Depth Func LEQUAL, CullMode NONE
    desc.IsDepthMask = false;
    desc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    desc.CullMode = CullMode::None;
    
    m_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    m_pPipelineState = ShaderManager::Instance().GetPipelineState(m_pProgram, desc);
}

void SkyShader::Begin(RenderContext& context)
{
    GraphicsShader::Begin(context);

    // Bind PerCamera
    int b0 = GetRootParameterIndex(ShaderBindingType::CBV, 0);
    if (b0 != -1) context.BindCamera(b0);
}

void SkyShader::BeginNode(const ModelData::Node& node, const Math::Matrix& nodeWorld)
{
    CBufferData::PerDraw cbDraw;
    cbDraw.mWorld = nodeWorld;
    
    // Bind PerDraw
    int b1 = GetRootParameterIndex(ShaderBindingType::CBV, 1);
    if (b1 != -1) GDF::Instance().BindCBuffer(b1, cbDraw);
}

void SkyShader::BeforeDrawMesh(const Mesh& mesh, const Material& material)
{
    SetMaterial(material);
}

void SkyShader::SetMaterial(const Material& material)
{
    // Bind Texture to t0
    int t0 = GetRootParameterIndex(ShaderBindingType::SRV, 0);
    if (t0 != -1) {
        auto* pTex = TextureManager::Instance().Get(material.BaseColor.handle);
        if (pTex && material.BaseColor.valid && pTex->IsReady()) {
            pTex->Set(t0);
        } else {
            GraphicsDevice::Instance().GetWhiteTex()->Set(t0);
        }
    }
}
