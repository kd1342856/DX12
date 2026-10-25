#include "../../Pch.h"
#include "ResourceStateTracker.h"
#include "../GPUResource/GPUResource.h"

void ResourceStateTracker::AddResourceState(ID3D12Resource* pResource, D3D12_RESOURCE_STATES initialState)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	if (pResource)
	{
		m_resourceStates[pResource] = { initialState, false };
	}
}

void ResourceStateTracker::RemoveResourceState(ID3D12Resource* pResource)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	if (pResource)
	{
		m_resourceStates.erase(pResource);
	}
}

ResourceState ResourceStateTracker::GetCurrentState(ID3D12Resource* pResource) const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_resourceStates.find(pResource);
	if (it != m_resourceStates.end())
	{
		return it->second;
	}
	// Default
	return { D3D12_RESOURCE_STATE_COMMON, false };
}

void ResourceStateTracker::SetCurrentState(ID3D12Resource* pResource, D3D12_RESOURCE_STATES state)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	if (pResource)
	{
		m_resourceStates[pResource] = { state, true };
	}
}
