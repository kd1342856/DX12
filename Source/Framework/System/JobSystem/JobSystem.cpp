#include "JobSystem.h"

JobSystem::~JobSystem() {
    Shutdown();
}

void JobSystem::Init() {
    uint32_t numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 4;
    
    // Reserve one thread for main, use the rest (at least 1)
    numThreads = (numThreads > 1) ? numThreads - 1 : 1;

    for (uint32_t i = 0; i < numThreads; ++i) {
        m_workers.emplace_back(&JobSystem::WorkerThread, this);
    }
}

void JobSystem::Shutdown() {
    if (m_stop) return;
    
    {
        std::unique_lock<std::mutex> lock(m_queueMutex);
        m_stop = true;
    }
    
    m_condition.notify_all();
    
    for (std::thread& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    m_workers.clear();
}

void JobSystem::Execute(std::function<void()> job) {
    {
        std::unique_lock<std::mutex> lock(m_queueMutex);
        m_jobQueue.push(job);
        m_activeJobs++;
    }
    m_condition.notify_one();
}

void JobSystem::Wait() {
    std::unique_lock<std::mutex> lock(m_queueMutex);
    m_waitCondition.wait(lock, [this] { return m_activeJobs == 0; });
}

void JobSystem::WorkerThread() {
    while (true) {
        std::function<void()> job;
        
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_condition.wait(lock, [this] { return m_stop || !m_jobQueue.empty(); });
            
            if (m_stop && m_jobQueue.empty()) {
                return;
            }
            
            job = std::move(m_jobQueue.front());
            m_jobQueue.pop();
        }
        
        // Execute the job outside of the lock
        job();
        
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_activeJobs--;
            if (m_activeJobs == 0) {
                m_waitCondition.notify_all();
            }
        }
    }
}
JobSystem& JobSystem::Instance()
{
    static JobSystem instance;
    return instance;
}
