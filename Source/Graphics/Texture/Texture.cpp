#include "Texture.h"

bool Texture::Load(GraphicsDevice* pGraphicsDevice, const std::string& filePath)
{
	m_pDevice = pGraphicsDevice;

	wchar_t wFilePath[128];
	MultiByteToWideChar(CP_ACP, 0, filePath.c_str(), -1, wFilePath, _countof(wFilePath));

	DirectX::TexMetadata metadata = {};
	DirectX::ScratchImage scratchImage = {};
	const DirectX::Image* pImage = nullptr;

	auto hr = DirectX::LoadFromWICFile(wFilePath, DirectX::WIC_FLAGS_NONE, &metadata, scratchImage);
	if (FAILED(hr))
	{
		assert("テクスチャの読み込み失敗");
		return false;
	}

	pImage = scratchImage.GetImage(0, 0, 0);

	D3D12_HEAP_PROPERTIES heapProp = {};
	heapProp.Type = D3D12_HEAP_TYPE_CUSTOM;
	heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
	D3D12_RESOURCE_DESC recDesc = {};
	recDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension);
	recDesc.Format = metadata.format;
	recDesc.Width = (UINT64)metadata.width;
	recDesc.Height = (UINT)metadata.height;
	recDesc.DepthOrArraySize = (UINT16)metadata.arraySize;
	recDesc.MipLevels = (UINT16)metadata.mipLevels;
	recDesc.SampleDesc.Count = 1;

	hr = m_pDevice->GetDevice()->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &recDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_pBuffer));

	if (FAILED(hr))
	{
		assert(0 && "テクスチャバッファ作成失敗");
		return false;
	}

	hr = m_pBuffer->WriteToSubresource(0, nullptr, pImage->pixels, (UINT)pImage->rowPitch, (UINT)pImage->slicePitch);
	if (FAILED(hr))
	{
		assert(0 && "バッファにテクスチャデータの書き込み失敗");
		return false;
	}

	m_srvNumber = m_pDevice->GetCBVSRVUAVHeap()->CreateSRV(m_pBuffer.Get());

	return true;
}

void Texture::Set(int index)
{
	m_pDevice->GetCmdList()->SetGraphicsRootDescriptorTable
	(index, m_pDevice->GetCBVSRVUAVHeap()->GetGPUHandle(m_srvNumber));
}
