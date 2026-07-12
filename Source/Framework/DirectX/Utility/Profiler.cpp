#include "../../../Pch.h"
#include "Profiler.h"
#include <windows.h>
#include <psapi.h>

float Profiler::GetSystemRAMUsageMB() const
{
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
    {
        return static_cast<float>(pmc.WorkingSetSize) / (1024.0f * 1024.0f);
    }
    return 0.0f;
}
