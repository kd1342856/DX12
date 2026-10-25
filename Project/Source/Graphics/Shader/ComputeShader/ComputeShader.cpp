#include "../../../Pch.h"
#include "ComputeShader.h"
#include "../ShaderCompiler/ShaderCompiler.h"

namespace
{
    D3D12_STATIC_SAMPLER_DESC MakeClampSampler(D3D12_FILTER filter, UINT shaderRegister)
    {
        D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = filter;
        samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
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

    D3D12_ROOT_PARAMETER1 rootParameters[2] = {};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParameters[0].Constants.ShaderRegister = 0;
    rootParameters[0].Constants.Num32BitValues = kMaxRootConstants;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].Descriptor.ShaderRegister = 1;
    rootParameters[1].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    const D3D12_STATIC_SAMPLER_DESC samplers[] = {
        MakeClampSampler(D3D12_FILTER_MIN_MAG_MIP_POINT, 0),
        MakeClampSampler(D3D12_FILTER_MIN_MAG_MIP_LINEAR, 1),
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

void ComputeShader::SetConstantBufferAddress(D3D12_GPU_VIRTUAL_ADDRESS address) const
{
    GraphicsDevice::Instance().GetCmdList()->SetComputeRootConstantBufferView(1, address);
}

void ComputeShader::Dispatch(UINT groupCountX, UINT groupCountY, UINT groupCountZ) const
{
    // 直前に積んだリソースの状態遷移を先に流す
    GraphicsDevice::Instance().GetContextManager()->GetGraphicsContext()->FlushResourceBarriers();
    GraphicsDevice::Instance().GetCmdList()->Dispatch(groupCountX, groupCountY, groupCountZ);
}
