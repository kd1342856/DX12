#include "../../Pch.h"
#include "ResourceStateTracker.h"
#include "../GPUResource/GPUResource.h"

void ResourceStateTracker::AddResourceState(ID3D12Resource* pResource, D3D12_RESOURCE_STATES initialState)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	if (pResource)
	{
		m_resourceStates[pResource] = initialState;
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

void ResourceStateTracker::TransitionResource(ID3D12Resource* pResource, D3D12_RESOURCE_STATES stateAfter)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	if (!pResource) return;

	auto it = m_resourceStates.find(pResource);
	if (it != m_resourceStates.end())
	{
		D3D12_RESOURCE_STATES stateBefore = it->second;

		if (stateBefore != stateAfter)
		{
			D3D12_RESOURCE_BARRIER barrier = {};
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barrier.Transition.pResource = pResource;
			barrier.Transition.StateBefore = stateBefore;
			barrier.Transition.StateAfter = stateAfter;
			barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

			m_pendingResourceBarriers.push_back(barrier);

			it->second = stateAfter;
		}
	}
	else
	{
		// Not tracked yet, track it assuming its initial state is stateAfter (or just add it)
		// Usually we expect AddResourceState to be called upon creation
		m_resourceStates[pResource] = stateAfter;
	}
}

void ResourceStateTracker::TransitionResource(GPUResource* pResource, D3D12_RESOURCE_STATES stateAfter)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	if (pResource && pResource->GetResource())
	{
		TransitionResource(pResource->GetResource(), stateAfter);
	}
}

void ResourceStateTracker::FlushResourceBarriers(ID3D12GraphicsCommandList* pCmdList)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	if (!m_pendingResourceBarriers.empty())
	{
		pCmdList->ResourceBarrier((UINT)m_pendingResourceBarriers.size(), m_pendingResourceBarriers.data());
		m_pendingResourceBarriers.clear();
	}
}
