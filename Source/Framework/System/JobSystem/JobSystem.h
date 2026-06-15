#pragma once
#include <functional>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

class JobSystem {
public:
    static JobSystem& Instance() {
        static JobSystem instance;
        return instance;
    }

    void Init();
    void Shutdown();

    // Execute a job asynchronously
    void Execute(std::function<void()> job);

    // Wait for all currently executing and queued jobs to finish
    void Wait();

private:
    JobSystem() = default;
    ~JobSystem();

    void WorkerThread();

    std::vector<std::thread> m_workers;
    std::queue<std::function<void()>> m_jobQueue;
    
    std::mutex m_queueMutex;
    std::condition_variable m_condition;
    
    std::atomic<bool> m_stop{false};
    std::atomic<int> m_activeJobs{0};
    std::condition_variable m_waitCondition;
};