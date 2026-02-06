#pragma once
class RTVHeap : public Heap<int>
{
public:

	RTVHeap(){}
	~RTVHeap(){}
	
	int CreateRTV(ID3D12Resource* pBuffer);

private:
};
