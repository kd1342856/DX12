#include "../../Pch.h"
#include "GPUUploadQueue.h"

void GPUUploadQueue::Submit(std::function<void()> task)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_queue.push_back(std::move(task));
}

void GPUUploadQueue::Process()
{
	std::vector<std::function<void()>> currentQueue;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		currentQueue.swap(m_queue);
	}

	for (auto& task : currentQueue) {
		task();
	}
}

void GPUUploadQueue::Clear()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_queue.clear();
}
