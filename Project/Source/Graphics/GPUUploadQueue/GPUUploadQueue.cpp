#include "../../Pch.h"
#include "GPUUploadQueue.h"
#include "../Device/GraphicsDevice.h"
#include "../Device/ResourceUploader.h"
#include "../../Framework/Manager/Asset/TextureManager.h"
#include "../../Framework/Manager/Asset/MeshManager.h"
#include "../../Framework/DirectX/Utility/Thread.h"

GPUUploadQueue& GPUUploadQueue::Instance()
{
    static GPUUploadQueue inst;
    return inst;
}

void GPUUploadQueue::Submit(UploadRequest request)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_queue.push_back(std::move(request));
}

void GPUUploadQueue::Process()
{
    // メインスレッドからのみ呼ばれること
    assert(Thread::IsMainThread() && "GPUUploadQueue::Process must be called on the main thread!");

    std::vector<UploadRequest> currentQueue;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_queue.empty()) return;
        currentQueue.swap(m_queue);
    }

    auto& uploader = *GraphicsDevice::Instance().GetResourceUploader();

    uploader.Begin();

    for (auto& request : currentQueue)
    {
        std::visit([&](auto& data)
        {
            using T = std::decay_t<decltype(data)>;

            if constexpr (std::is_same_v<T, TextureUploadData>)
            {
                auto* pTexture = TextureManager::Instance().GetRaw(data.targetHandle);
                if (pTexture)
                {
                    pTexture->CreateGPU(&GraphicsDevice::Instance(), data);
                    pTexture->SetState(AssetState::Ready);
                }
            }
            else if constexpr (std::is_same_v<T, MeshUploadData>)
            {
                auto* pMesh = MeshManager::Instance().GetRaw(data.targetHandle);
                if (pMesh)
                {
                    pMesh->CreateGPU(&GraphicsDevice::Instance(), data.vertices, data.faces, data.material);
                    pMesh->SetState(AssetState::Ready);
                }
            }
            else if constexpr (std::is_same_v<T, std::function<void()>>)
            {
                if (data)
                {
                    try {
                        data();
                    } catch (const std::exception& e) {
                        std::ofstream ofs("exception.log", std::ios::app);
                        ofs << "Exception in GPUUploadQueue::Process: " << e.what() << "\n";
                        ofs.close();
                    } catch (...) {
                        std::ofstream ofs("exception.log", std::ios::app);
                        ofs << "Unknown exception in GPUUploadQueue::Process\n";
                        ofs.close();
                    }
                }
            }

        }, request.data);
    }

    uploader.End();
}

void GPUUploadQueue::Clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_queue.clear();
}
