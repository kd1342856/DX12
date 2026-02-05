#pragma once
class DSVHeap : public Heap<int>
{
public:
	DSVHeap(){}
	~DSVHeap(){}

	int CreateDSV(ID3D12Resource* pBuffer, DXGI_FORMAT format);
private:

};
