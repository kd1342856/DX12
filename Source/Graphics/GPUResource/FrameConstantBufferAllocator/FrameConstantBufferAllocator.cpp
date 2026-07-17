#include "../../../Pch.h"
#include "FrameConstantBufferAllocator.h"

FrameConstantBufferAllocator::~FrameConstantBufferAllocator()
{
	if (m_resource && m_pMappedBuffer)
	{
		m_resource->Unmap(0, nullptr);
	}
}

bool FrameConstantBufferAllocator::Create(GraphicsDevice* pGraphicsDevice, UINT capacityPerFrame, UINT frameCount)
{
	
	m_device = pGraphicsDevice;
	m_capacityPerFrame = capacityPerFrame;
	UINT totalCapacity = m_capacityPerFrame;
	
	// kFrameCount は GraphicsDevice 側で定義されている定数。
	// ここでは固定で2(または3)ですが、GraphicsDevice::kFrameCount を参照するために
	// #include "../../Device/GraphicsDevice.h" が必要かもしれません。
	// この cpp の先頭で #include "../../Device/GraphicsDevice.h" をしているか確認するか、
	// ここではマジックナンバーを避けるため Device から直接取得するか、定数とします。
	// GraphicsDeviceのクラス定義を参照する必要がありますが、とりあえず3としますか。
	// いや、前見た GraphicsDevice.cpp に `for (int i = 0; i < kFrameCount; ++i)` とあったので
	// GraphicsDevice::kFrameCount を使います。

	D3D12_HEAP_PROPERTIES heapprop = {};
	heapprop.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resDesc.Height = 1;
	resDesc.DepthOrArraySize = 1;
	resDesc.MipLevels = 1;
	resDesc.SampleDesc.Count = 1;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resDesc.Width = 256ULL * totalCapacity;

	HRESULT hr = m_device->GetDevice()->CreateCommittedResource(
		&heapprop, D3D12_HEAP_FLAG_NONE, &resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_resource));

	if (FAILED(hr))
	{
		return false;
	}

	m_resource->Map(0, nullptr, (void**)&m_pMappedBuffer);

	auto* pAllocator = m_device->GetDescriptorHeapManager()->GetCBVSRVUAVAllocator();
	
	for (UINT i = 0; i < totalCapacity; ++i)
	{
		UINT currentIndex = pAllocator->Allocate();
        if (i == 0) m_descriptorBaseIndex = currentIndex;
		
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbDesc = {};
		cbDesc.BufferLocation = m_resource->GetGPUVirtualAddress() + (UINT64)i * 256;
		cbDesc.SizeInBytes = 256;
		
		m_device->GetDevice()->CreateConstantBufferView(&cbDesc, pAllocator->GetCPUHandle(currentIndex));
	}

	return true;
}

void FrameConstantBufferAllocator::Reset()
{
	m_currentUseNumber = 0;
}
