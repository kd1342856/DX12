#pragma once

class CBVSRVUAVHeap : public Heap<Math::Vector3>
{
public:

	int CreateSRV(ID3D12Resource* pBuffer);

	const D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(int number)override;

	void SetHeap();

private:

};