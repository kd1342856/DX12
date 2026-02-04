#pragma once

class CBVSRVUAVHeap : public Heap<Math::Vector3>
{
public:
	CBVSRVUAVHeap(){}
	~CBVSRVUAVHeap(){}

	int CreateSRV(ID3D12Resource* pBuffer);

	const D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(int number)override;

	void SetHeap();

	ID3D12DescriptorHeap* GetHeap() { return m_pHeap.Get(); }

	const Math::Vector3& GetUseCount() { return m_useCount; }

private:

};