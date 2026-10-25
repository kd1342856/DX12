#include "../../../Pch.h"
#include "ComputeShader.h"
#include "../ShaderCompiler/ShaderCompiler.h"

namespace
{
    D3D12_STATIC_SAMPLER_DESC MakeSampler(D3D12_FILTER filter, D3D12_TEXTURE_ADDRESS_MODE addressMode, UINT shaderRegister)
    {
        D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = filter;
        samplerDesc.AddressU = addressMode;
        samplerDesc.AddressV = addressMode;
        samplerDesc.AddressW = addressMode;
        samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        samplerDesc.MaxAnisotropy = 1;
        samplerDesc.MinLOD = 0.0f;
        samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
        samplerDesc.ShaderRegister = shaderRegister;
        samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        return samplerDesc;
    }
}

void ComputeShader::InitializeShared(ID3D12Device* pDevice)
{
    s_isSupported = false;

    // シェーダーモデル6.6(ResourceDescriptorHeap)と、リソースバインディングのTier 3が要る
    D3D12_FEATURE_DATA_SHADER_MODEL shaderModel = { D3D_SHADER_MODEL_6_6 };
    if (FAILED(pDevice->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof(shaderModel))) ||
        shaderModel.HighestShaderModel < D3D_SHADER_MODEL_6_6)
    {
        Logger::Instance().AddLog(Logger::LogLevel::Warning, u8"[ComputeShader] シェーダーモデル6.6に対応していないため、TAAなどを無効にします");
        return;
    }
    D3D12_FEATURE_DATA_D3D12_OPTIONS options = {};
    if (FAILED(pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options))) ||
        options.ResourceBindingTier < D3D12_RESOURCE_BINDING_TIER_3)
    {
        Logger::Instance().AddLog(Logger::LogLevel::Warning, u8"[ComputeShader] リソースバインディングのTier 3に対応していないため、TAAなどを無効にします");
        return;
    }

    // [0]が32ビットの定数(b0)、[1]～[4]が定数バッファ(b1～b4)
    D3D12_ROOT_PARAMETER1 rootParameters[1 + kLastConstantBufferRegister] = {};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParameters[0].Constants.ShaderRegister = 0;
    rootParameters[0].Constants.Num32BitValues = kMaxRootConstants;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    for (UINT shaderRegister = 1; shaderRegister <= kLastConstantBufferRegister; ++shaderRegister)
    {
        D3D12_ROOT_PARAMETER1& parameter = rootParameters[shaderRegister];
        parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        parameter.Descriptor.ShaderRegister = shaderRegister;
        parameter.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;
        parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    }

    const D3D12_STATIC_SAMPLER_DESC samplers[] = {
        MakeSampler(D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 0),
        MakeSampler(D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 1),
        MakeSampler(D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, 2),
    };

    D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    rootSignatureDesc.Desc_1_1.NumParameters = _countof(rootParameters);
    rootSignatureDesc.Desc_1_1.pParameters = rootParameters;
    rootSignatureDesc.Desc_1_1.NumStaticSamplers = _countof(samplers);
    rootSignatureDesc.Desc_1_1.pStaticSamplers = samplers;
    rootSignatureDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;

    Microsoft::WRL::ComPtr<ID3DBlob> cpBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> cpErrorBlob;
    if (FAILED(D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &cpBlob, &cpErrorBlob)))
    {
        Logger::Instance().AddLog(Logger::LogLevel::Error, u8"[ComputeShader] ルートシグネチャを作れませんでした: %s",
            cpErrorBlob ? static_cast<const char*>(cpErrorBlob->GetBufferPointer()) : "");
        return;
    }
    if (FAILED(pDevice->CreateRootSignature(0, cpBlob->GetBufferPointer(), cpBlob->GetBufferSize(), IID_PPV_ARGS(&s_cpRootSignature))))
    {
        Logger::Instance().AddLog(Logger::LogLevel::Error, u8"[ComputeShader] ルートシグネチャを作れませんでした");
        return;
    }
    s_cpRootSignature->SetName(L"BindlessComputeRootSignature");
    s_isSupported = true;
}

void ComputeShader::ReleaseShared()
{
    s_cpRootSignature.Reset();
    s_isSupported = false;
}

bool ComputeShader::Create(const std::wstring& filePath, const std::wstring& entryPoint)
{
    m_cpPipelineState.Reset();
    if (!s_isSupported)
    {
        return false;
    }

    Microsoft::WRL::ComPtr<IDxcBlob> cpBlob = ShaderCompiler::CompileCS(filePath, entryPoint, L"cs_6_6");
    if (!cpBlob)
    {
        return false;
    }

    D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineDesc = {};
    pipelineDesc.pRootSignature = s_cpRootSignature.Get();
    pipelineDesc.CS.pShaderBytecode = cpBlob->GetBufferPointer();
    pipelineDesc.CS.BytecodeLength = cpBlob->GetBufferSize();
    if (FAILED(GraphicsDevice::Instance().GetDevice()->CreateComputePipelineState(&pipelineDesc, IID_PPV_ARGS(&m_cpPipelineState))))
    {
        Logger::Instance().AddLog(Logger::LogLevel::Error, u8"[ComputeShader] PSOを作れませんでした: %ls", filePath.c_str());
        m_cpPipelineState.Reset();
        return false;
    }
    return true;
}

void ComputeShader::Begin() const
{
    auto* pCmdList = GraphicsDevice::Instance().GetCmdList();
    // ResourceDescriptorHeapで参照するヒープは、フレームの先頭(GraphicsDevice::BeginFrame)で設定済み
    pCmdList->SetComputeRootSignature(s_cpRootSignature.Get());
    pCmdList->SetPipelineState(m_cpPipelineState.Get());
}

void ComputeShader::SetConstantsRaw(const void* pData, UINT count) const
{
    GraphicsDevice::Instance().GetCmdList()->SetComputeRoot32BitConstants(0, count, pData, 0);
}

void ComputeShader::SetConstantBufferAddress(UINT shaderRegister, D3D12_GPU_VIRTUAL_ADDRESS address) const
{
    assert(shaderRegister >= 1 && shaderRegister <= kLastConstantBufferRegister && "定数バッファのレジスタはb1～b4");
    // ルートパラメーターの番号はレジスタの番号と同じ
    GraphicsDevice::Instance().GetCmdList()->SetComputeRootConstantBufferView(shaderRegister, address);
}

void ComputeShader::Dispatch(UINT groupCountX, UINT groupCountY, UINT groupCountZ) const
{
    // 直前に積んだリソースの状態遷移を先に流す
    GraphicsDevice::Instance().GetContextManager()->GetGraphicsContext()->FlushResourceBarriers();
    GraphicsDevice::Instance().GetCmdList()->Dispatch(groupCountX, groupCountY, groupCountZ);
}
