#include "../../../Pch.h"
#include "CommandContext.h"
#include "../../GPUResource/GPUResource.h"
#include "../GraphicsDevice.h"
#include "../ResourceStateTracker.h"

void CommandContext::TransitionResource(ID3D12Resource* pResource, D3D12_RESOURCE_STATES stateAfter)
{
    if (!pResource) return;

    D3D12_RESOURCE_STATES stateBefore;

    bool foundInPending = false;

    // Check if we have a pending state for this resource in the current context (search backwards)
    for (auto it = m_pendingStates.rbegin(); it != m_pendingStates.rend(); ++it)
    {
        if (it->resource == pResource)
        {
            stateBefore = it->after;
            foundInPending = true;
            break;
        }
    }

    if (!foundInPending)
    {
        // Query the global tracker for the current state
        ResourceState rs = GraphicsDevice::Instance().GetResourceStateTracker()->GetCurrentState(pResource);
        stateBefore = rs.state;
    }

    if (stateBefore != stateAfter)
    {
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = pResource;
        barrier.Transition.StateBefore = stateBefore;
        barrier.Transition.StateAfter = stateAfter;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        m_pendingBarriers.push_back(barrier);
        m_pendingStates.push_back({ pResource, stateBefore, stateAfter });
    }
}

void CommandContext::TransitionResource(GPUResource* pResource, D3D12_RESOURCE_STATES stateAfter)
{
    if (pResource && pResource->GetResource())
    {
        TransitionResource(pResource->GetResource(), stateAfter);
    }
}

void CommandContext::FlushResourceBarriers()
{
    if (!m_pendingBarriers.empty())
    {
        m_pCmdList->ResourceBarrier((UINT)m_pendingBarriers.size(), m_pendingBarriers.data());
        m_pendingBarriers.clear();
    }
}

void CommandContext::ResolvePendingStates()
{
    auto* pGlobalTracker = GraphicsDevice::Instance().GetResourceStateTracker();
    if (pGlobalTracker)
    {
        for (const auto& transition : m_pendingStates)
        {
            pGlobalTracker->SetCurrentState(transition.resource, transition.after);
        }
    }
    m_pendingStates.clear();
}
