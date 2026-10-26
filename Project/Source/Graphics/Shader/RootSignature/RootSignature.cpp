#include "../../../Pch.h"
#include "RootSignature.h"
#include "../ComputeShader/ComputeShader.h"

void RootSignature::Create(GraphicsDevice* pGraphicsDevice, const std::vector<DescriptorRange>& inputRanges)
{
	m_pDevice = pGraphicsDevice;

	// 版1.1で作る(シェーダーモデル6.6のヒープを直接参照するフラグは1.0では作れない)。
	// ディスクリプタとデータは「揮発」にして、1.0と同じ扱い(描画の途中で書き換えてよい)にする
	int rangeCount = (int)inputRanges.size();
	std::vector<D3D12_ROOT_PARAMETER1> rootParams(rangeCount);
	std::vector<D3D12_DESCRIPTOR_RANGE1> dxRanges(rangeCount);

	bool bSampler = false;

	for (int i = 0; i < rangeCount; ++i) {
		dxRanges[i] = {};
		dxRanges[i].NumDescriptors = inputRanges[i].Count;
		dxRanges[i].BaseShaderRegister = inputRanges[i].Register;
		dxRanges[i].RegisterSpace = inputRanges[i].Space;
		dxRanges[i].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		dxRanges[i].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;

		switch (inputRanges[i].Type) {
		case RangeType::CBV:
			dxRanges[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
			break;
		case RangeType::SRV:
			dxRanges[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			bSampler = true;
			break;
		case RangeType::UAV:
			dxRanges[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
			break;
		}

		rootParams[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[i].DescriptorTable.pDescriptorRanges = &dxRanges[i];
		rootParams[i].DescriptorTable.NumDescriptorRanges = 1;
		rootParams[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	}

	std::array<D3D12_STATIC_SAMPLER_DESC, 7> pStaticSmplerDesc = {};
	if (bSampler) {
		CreateStaticSampler(pStaticSmplerDesc[0], TextureAddressMode::Wrap, D3D12Filter::Linear, 0); 
		CreateStaticSampler(pStaticSmplerDesc[1], TextureAddressMode::Clamp, D3D12Filter::Linear, 1);
		CreateStaticSampler(pStaticSmplerDesc[2], TextureAddressMode::Wrap, D3D12Filter::Linear, 2);
		CreateStaticSampler(pStaticSmplerDesc[3], TextureAddressMode::Clamp, D3D12Filter::Linear, 3);
		CreateStaticSampler(pStaticSmplerDesc[4], TextureAddressMode::Wrap, D3D12Filter::Point, 4);
		CreateStaticSampler(pStaticSmplerDesc[5], TextureAddressMode::Clamp, D3D12Filter::Point, 5);

		D3D12_STATIC_SAMPLER_DESC cmpSampler = {};
		cmpSampler.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
		cmpSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		cmpSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		cmpSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		cmpSampler.MipLODBias = 0.0f;
		cmpSampler.MaxAnisotropy = 1;
		cmpSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		cmpSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
		cmpSampler.MinLOD = 0.0f;
		cmpSampler.MaxLOD = D3D12_FLOAT32_MAX;
		cmpSampler.ShaderRegister = 15;
		cmpSampler.RegisterSpace = 0;
		cmpSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		pStaticSmplerDesc[6] = cmpSampler;
	}

	D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
	rootSignatureDesc.pStaticSamplers = bSampler ? pStaticSmplerDesc.data() : nullptr;
	rootSignatureDesc.NumStaticSamplers = bSampler ? 7 : 0;
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	// シェーダーモデル6.6に対応していれば、描画のシェーダーからもResourceDescriptorHeapで読めるようにする
	// (レイトレースの影と反射で、加速構造と当たった物の頂点・テクスチャを番号で読む)
	if (ComputeShader::IsSupported())
	{
		rootSignatureDesc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;
	}
	rootSignatureDesc.pParameters = rootParams.data();
	rootSignatureDesc.NumParameters = (int)inputRanges.size();

	D3D12_VERSIONED_ROOT_SIGNATURE_DESC versionedDesc = {};
	versionedDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	versionedDesc.Desc_1_1 = rootSignatureDesc;
	Microsoft::WRL::ComPtr<ID3DBlob> cpErrorBlob;
	auto hr = D3D12SerializeVersionedRootSignature(&versionedDesc, &m_pRootBlob, &cpErrorBlob);
	if (FAILED(hr))
	{
		OutputDebugStringA("SerializeRootSignature Failed\n");
		Logger::Instance().AddLog(Logger::LogLevel::Error, u8"[RootSignature] ルートシグネチャを作れませんでした: %s",
			cpErrorBlob ? static_cast<const char*>(cpErrorBlob->GetBufferPointer()) : "");
		return;
	}
	hr = m_pDevice->GetDevice()->CreateRootSignature(0, m_pRootBlob->GetBufferPointer(), m_pRootBlob->GetBufferSize(), IID_PPV_ARGS(m_pRootSignature.GetAddressOf()));
	if (FAILED(hr))
	{
		OutputDebugStringA("CreateRootSignature Failed\n");
		return;
	}
}

void RootSignature::CreateStaticSampler(D3D12_STATIC_SAMPLER_DESC& pSamplerDesc, TextureAddressMode mode, D3D12Filter filter, int count)
{
	D3D12_TEXTURE_ADDRESS_MODE addressMode = mode == TextureAddressMode::Wrap ? D3D12_TEXTURE_ADDRESS_MODE_WRAP : D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	D3D12_FILTER samplingFilter = filter == D3D12Filter::Point ? D3D12_FILTER_MIN_MAG_MIP_POINT : D3D12_FILTER_MIN_MAG_MIP_LINEAR;

	pSamplerDesc = {};
	pSamplerDesc.AddressU = addressMode;
	pSamplerDesc.AddressV = addressMode;
	pSamplerDesc.AddressW = addressMode;
	pSamplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	pSamplerDesc.Filter = samplingFilter;
	pSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
	pSamplerDesc.MinLOD = 0.0f;
	pSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	pSamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	pSamplerDesc.MaxAnisotropy = 16;
	pSamplerDesc.ShaderRegister = count;
}