#pragma once
#include <mutex>
#include <unordered_map>
#include <d3d12.h>

struct ResourceState
{
	D3D12_RESOURCE_STATES state;
	bool initialized;
};

class GPUResource;

class ResourceStateTracker
{
public:
	ResourceStateTracker() = default;
	~ResourceStateTracker() = default;

	// Global state tracking
	void AddResourceState(ID3D12Resource* pResource, D3D12_RESOURCE_STATES initialState);
	void RemoveResourceState(ID3D12Resource* pResource);

	ResourceState GetCurrentState(ID3D12Resource* pResource) const;
	void SetCurrentState(ID3D12Resource* pResource, D3D12_RESOURCE_STATES state);

private:
	mutable std::mutex m_mutex;
	std::unordered_map<ID3D12Resource*, ResourceState> m_resourceStates;
};
