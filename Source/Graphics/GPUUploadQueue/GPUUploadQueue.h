#pragma once
#include <mutex>
#include <vector>
#include <functional>

// Singleton queue for managing GPU uploads safely from the main thread
class GPUUploadQueue
{
public:
	static GPUUploadQueue& Instance() { static GPUUploadQueue inst; return inst; }

	// Submit a task (usually a resource upload via ResourceUploader) from a worker thread
	void Submit(std::function<void()> task);

	// Called on the main thread (e.g. before rendering) to execute all pending upload tasks
	void Process();

	// Clear all pending tasks
	void Clear();

private:
	std::mutex m_mutex;
	std::vector<std::function<void()>> m_queue;
};
