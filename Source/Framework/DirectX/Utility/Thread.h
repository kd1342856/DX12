#pragma once
#include <thread>

namespace Thread
{
    extern std::thread::id g_mainThreadId;

    inline void RegisterMainThread()
    {
        g_mainThreadId = std::this_thread::get_id();
    }

    inline bool IsMainThread()
    {
        return std::this_thread::get_id() == g_mainThreadId;
    }
}
