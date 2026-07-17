#pragma once
#include <mutex>
#include <unordered_map>
#include <vector>
#include <d3d12.h>

class GPUResource;

class ResourceStateTracker
{
public:
	ResourceStateTracker() = default;
	~ResourceStateTracker() = default;

	void AddResourceState(ID3D12Resource* pResource, D3D12_RESOURCE_STATES initialState);
	void RemoveResourceState(ID3D12Resource* pResource);

	void TransitionResource(ID3D12Resource* pResource, D3D12_RESOURCE_STATES stateAfter);
	void TransitionResource(GPUResource* pResource, D3D12_RESOURCE_STATES stateAfter);

	void FlushResourceBarriers(ID3D12GraphicsCommandList* pCmdList);

private:
	std::unordered_map<ID3D12Resource*, D3D12_RESOURCE_STATES> m_resourceStates;
	std::vector<D3D12_RESOURCE_BARRIER> m_pendingResourceBarriers;
	std::mutex m_mutex;
};
