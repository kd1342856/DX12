#pragma once
#include <condition_variable>

class JobSystem {
public:
    static JobSystem& Instance();

    void Init();
    void Shutdown();

    // Execute a job asynchronously
    void Execute(std::function<void()> job);

    // Wait for all currently executing and queued jobs to finish
    void Wait();

    // Statistics
    int GetActiveJobCount() const { return m_activeJobs.load(); }
    size_t GetWorkerCount() const { return m_workers.size(); }
    std::vector<bool> GetWorkerStatuses();
    size_t GetQueuedJobCount();

private:
    JobSystem() = default;
    ~JobSystem();

    void WorkerThread(uint32_t workerIndex);

    std::vector<std::thread> m_workers;
    std::vector<bool> m_workerActive;
    std::queue<std::function<void()>> m_jobQueue;
    
    std::mutex m_queueMutex;
    std::condition_variable m_condition;
    
    std::atomic<bool> m_stop{false};
    std::atomic<bool> m_acceptJob{true};
    std::atomic<int> m_activeJobs{0};
    std::condition_variable m_waitCondition;
};
