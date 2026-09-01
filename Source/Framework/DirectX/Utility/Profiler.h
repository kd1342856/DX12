#pragma once

#include <cstdint>

class Profiler
{
public:
    static Profiler& Instance()
    {
        static Profiler instance;
        return instance;
    }

    void ResetPerFrameCounters()
    {
        m_drawCallCount = 0;
        m_drawCallBreakdown.clear();
        m_instanceCount = 0;
        m_instanceBreakdown.clear();
        m_dispatchCount = 0;
    }

    void AddDrawCall(const std::string& name, uint32_t instanceCount = 1)
    {
        m_drawCallCount++;
        m_drawCallBreakdown[name]++;
        m_instanceCount += instanceCount;
        m_instanceBreakdown[name] += instanceCount;
    }

    void AddDispatch()
    {
        m_dispatchCount++;
    }

    uint32_t GetDrawCallCount() const { return m_drawCallCount; }
    uint32_t GetInstanceCount() const { return m_instanceCount; }
    const std::unordered_map<std::string, uint32_t>& GetInstanceBreakdown() const { return m_instanceBreakdown; }
    uint32_t GetDispatchCount() const { return m_dispatchCount; }
    const std::unordered_map<std::string, uint32_t>& GetDrawCallBreakdown() const { return m_drawCallBreakdown; }

    // Memory query
    float GetSystemRAMUsageMB() const;

private:
    Profiler() = default;
    ~Profiler() = default;

    uint32_t m_drawCallCount = 0;
    std::unordered_map<std::string, uint32_t> m_drawCallBreakdown;
    uint32_t m_instanceCount = 0;
    std::unordered_map<std::string, uint32_t> m_instanceBreakdown;
    uint32_t m_dispatchCount = 0;
};
