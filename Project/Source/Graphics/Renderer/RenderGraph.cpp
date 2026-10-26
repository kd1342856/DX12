#include "../../Pch.h"
#include "RenderGraph.h"
#include "../../Framework/DirectX/Utility/Profiler.h"

void RenderGraph::AddPass(const std::string& name, std::function<void()> execute, std::vector<ResourceUse> uses, Profiling profiling)
{
    Pass pass;
    pass.name = name;
    pass.execute = std::move(execute);
    pass.uses = std::move(uses);
    pass.profiling = profiling;
    m_passes.push_back(std::move(pass));
}

void RenderGraph::Execute()
{
    auto& device = GraphicsDevice::Instance();
    auto* pContext = device.GetContextManager()->GetGraphicsContext();
    m_executedPassNames.clear();

    for (Pass& pass : m_passes)
    {
        m_executedPassNames.push_back(pass.name);
        // 宣言した資源を、パスの前に必要な状態にしておく
        for (const ResourceUse& use : pass.uses)
        {
            if (use.pResource)
            {
                pContext->TransitionResource(use.pResource, use.state);
            }
        }
        pContext->FlushResourceBarriers();

        if (pass.profiling == Profiling::CpuAndGpu)
        {
            PROFILE_CPU_SCOPE(pass.name);
            PROFILE_GPU_SCOPE(device.GetCmdList(), pass.name);
            pass.execute();
        }
        else
        {
            pass.execute();
        }
    }
    m_passes.clear();
}
