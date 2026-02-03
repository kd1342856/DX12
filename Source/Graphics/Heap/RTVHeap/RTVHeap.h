#pragma once
class RTVHeap : public Heap<int>
{
public:
	int CreateRTV(ID3D12Resource* pBuffer);

private:
};
