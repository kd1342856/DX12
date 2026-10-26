#pragma once

#include "../Buffer/GPUBuffer.h"

// 同じ中身の定数バッファを、そのフレームの中で使い回すための覚え書き(BindCachedDataに渡す)。
// 前に作ったアロケーター・フレームと中身を覚えておき、同じならディスクリプタをそのまま束縛し直す
struct ConstantBufferCache
{
	const void* pAllocator = nullptr;
	uint64_t serial = 0;
	std::vector<uint8_t> data;
	D3D12_GPU_DESCRIPTOR_HANDLE handle = {};
};

class FrameConstantBufferAllocator : public GPUBuffer
{
public:
	FrameConstantBufferAllocator() = default;
	~FrameConstantBufferAllocator();

	bool Create(GraphicsDevice* pGraphicsDevice, UINT capacityPerFrame = 10000, UINT frameCount = 2);

	template<typename T>
	void BindAndAttachData(int descIndex, const T& data);
	// BindAndAttachDataと同じだが、このフレームですでに同じ中身を作っていれば、それを使い回す。
	// 描画のたびに同じライト・カメラの定数を書き直して、フレームの容量を使い切らないようにする
	template<typename T>
	void BindCachedData(int descIndex, const T& data, ConstantBufferCache& cache);
	// このフレームのバッファにデータを写し、そのGPUアドレスを返す(ディスクリプタは作らない)。
	// ルートCBV(コンピュートシェーダーのSetComputeRootConstantBufferViewなど)に使う
	template<typename T>
	D3D12_GPU_VIRTUAL_ADDRESS AllocateData(const T& data);
	void Reset();

private:
	// データを写してCBVを作り、そのGPUハンドルを返す。容量が足りなければptrが0
	D3D12_GPU_DESCRIPTOR_HANDLE AllocateDescriptor(const void* pData, UINT size);

	UINT8* m_pMappedBuffer = nullptr;
	// Reset()のたびに増える。ConstantBufferCacheが、別のフレームで作ったものを使わないようにする
	uint64_t m_serial = 0;
	UINT m_currentUseNumber = 0;
	UINT m_capacityPerFrame = 0;
	UINT m_descriptorBaseIndex = 0;
	UINT m_currentFrameIndex = 0;
};

template<typename T>
inline void FrameConstantBufferAllocator::BindAndAttachData(int descIndex, const T& data)
{
	const D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = AllocateDescriptor(&data, sizeof(T));
	if (gpuHandle.ptr == 0)
	{
		return;
	}
	m_device->GetCmdList()->SetGraphicsRootDescriptorTable(descIndex, gpuHandle);
}

template<typename T>
inline void FrameConstantBufferAllocator::BindCachedData(int descIndex, const T& data, ConstantBufferCache& cache)
{
	const bool isSame = cache.pAllocator == this && cache.serial == m_serial &&
		cache.data.size() == sizeof(T) && memcmp(cache.data.data(), &data, sizeof(T)) == 0;
	if (!isSame)
	{
		const D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = AllocateDescriptor(&data, sizeof(T));
		if (gpuHandle.ptr == 0)
		{
			return;
		}
		cache.pAllocator = this;
		cache.serial = m_serial;
		cache.data.resize(sizeof(T));
		memcpy(cache.data.data(), &data, sizeof(T));
		cache.handle = gpuHandle;
	}
	m_device->GetCmdList()->SetGraphicsRootDescriptorTable(descIndex, cache.handle);
}

template<typename T>
inline D3D12_GPU_VIRTUAL_ADDRESS FrameConstantBufferAllocator::AllocateData(const T& data)
{
	int sizeAligned = (sizeof(T) + 255) & ~255;
	int useValue = sizeAligned / 256;

	if (m_currentUseNumber + useValue > m_capacityPerFrame)
	{
		assert(false && "Constant buffer capacity exceeded.");
		return 0;
	}

	int top = m_currentUseNumber;
	memcpy(m_pMappedBuffer + (size_t)top * 256, &data, sizeof(T));
	m_currentUseNumber += useValue;
	return m_resource->GetGPUVirtualAddress() + (UINT64)top * 256;
}
