#pragma once

#include "../Buffer/GPUBuffer.h"

class FrameConstantBufferAllocator : public GPUBuffer
{
public:
	FrameConstantBufferAllocator() = default;
	~FrameConstantBufferAllocator();

	bool Create(GraphicsDevice* pGraphicsDevice, UINT capacityPerFrame = 10000, UINT frameCount = 2);

	template<typename T>
	void BindAndAttachData(int descIndex, const T& data);
	void Reset();

private:
	UINT8* m_pMappedBuffer = nullptr;
	UINT m_currentUseNumber = 0;
	UINT m_capacityPerFrame = 0;
	UINT m_descriptorBaseIndex = 0;
	UINT m_currentFrameIndex = 0;
};

template<typename T>
inline void FrameConstantBufferAllocator::BindAndAttachData(int descIndex, const T& data)
{
	int sizeAligned = (sizeof(T) + 255) & ~255;
	int useValue = sizeAligned / 256;

	if (m_currentUseNumber + useValue > m_capacityPerFrame)
	{
		assert(false && "Constant buffer capacity exceeded.");
		return;
	}

	int top = m_currentUseNumber;
	memcpy(m_pMappedBuffer + (size_t)top * 256, &data, sizeof(T));

	auto* pAllocator = m_device->GetDescriptorHeapManager()->GetCBVSRVUAVAllocator();
	UINT cpuIndex = m_descriptorBaseIndex + top;

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbDesc = {};
	cbDesc.BufferLocation = m_resource->GetGPUVirtualAddress() + (UINT64)top * 256;
	cbDesc.SizeInBytes = sizeAligned;
	m_device->GetDevice()->CreateConstantBufferView(&cbDesc, pAllocator->GetCPUHandle(cpuIndex));

	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = pAllocator->GetGPUHandle(cpuIndex);

	m_device->GetCmdList()->SetGraphicsRootDescriptorTable(descIndex, gpuHandle);

	m_currentUseNumber += useValue;
}