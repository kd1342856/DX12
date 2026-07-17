#include "../../../Pch.h"
#include "../../Device/GraphicsDevice.h"
#include "../../Device/ResourceUploader.h"

bool Texture::Load(GraphicsDevice* pGraphicsDevice, const std::string& filePath)
{
	m_device = pGraphicsDevice;

	wchar_t wFilePath[128];
	MultiByteToWideChar(CP_ACP, 0, filePath.c_str(), -1, wFilePath, _countof(wFilePath));

	DirectX::TexMetadata metadata = {};
	DirectX::ScratchImage scratchImage = {};
	const DirectX::Image* pImage = nullptr;

	auto hr = DirectX::LoadFromWICFile(wFilePath, DirectX::WIC_FLAGS_NONE, &metadata, scratchImage);
	if (FAILED(hr))
	{
		assert(0 && "?e?N?X?`????????????s???????");
		return false;
	}

	pImage = scratchImage.GetImage(0, 0, 0);

	D3D12_HEAP_PROPERTIES heapProp = {};
	heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	D3D12_RESOURCE_DESC recDesc = {};
	recDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension);
	recDesc.Format = metadata.format;
	recDesc.Width = (UINT64)metadata.width;
	recDesc.Height = (UINT)metadata.height;
	recDesc.DepthOrArraySize = (UINT16)metadata.arraySize;
	recDesc.MipLevels = (UINT16)metadata.mipLevels;
	recDesc.SampleDesc.Count = 1;

	hr = m_device->GetDevice()->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &recDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_resource));

	if (FAILED(hr))
	{
		assert(0 && "?e?N?X?`???o?b?t?@???????s???????");
		return false;
	}

	hr = m_resource->WriteToSubresource(0, nullptr, pImage->pixels, (UINT)pImage->rowPitch, (UINT)pImage->slicePitch);
	if (FAILED(hr))
	{
		assert(0 && "?o?b?t?@??e?N?X?`???f?[?^?????????????????");
		return false;
	}

	m_srvNumber = m_device->CreateSRV(m_resource.Get());

	return true;
}

void Texture::Set(int index)
{
	m_device->GetCmdList()->SetGraphicsRootDescriptorTable
	(index, m_device->GetDescriptorHeapManager()->GetCBVSRVUAVAllocator()->GetGPUHandle(m_srvNumber));
}

bool Texture::CreateFromMemory(const void* data, int width, int height, DXGI_FORMAT format)
{
	m_device = &GraphicsDevice::Instance();

	D3D12_HEAP_PROPERTIES heapProp = {};
	heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	D3D12_RESOURCE_DESC recDesc = {};
	recDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	recDesc.Format = format;
	recDesc.Width = (UINT64)width;
	recDesc.Height = (UINT)height;
	recDesc.DepthOrArraySize = 1;
	recDesc.MipLevels = 1;
	recDesc.SampleDesc.Count = 1;

	auto hr = m_device->GetDevice()->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &recDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_resource));

	if (FAILED(hr))
	{
		assert(0 && "Failed to create texture buffer from memory");
		return false;
	}

	// For R8G8B8A8_UNORM, 4 bytes per pixel.
	UINT rowPitch = width * 4;
	UINT slicePitch = rowPitch * height;

	hr = m_resource->WriteToSubresource(0, nullptr, data, rowPitch, slicePitch);
	if (FAILED(hr))
	{
		assert(0 && "Failed to write texture data");
		return false;
	}

	m_srvNumber = m_device->CreateSRV(m_resource.Get());

	return true;
}
