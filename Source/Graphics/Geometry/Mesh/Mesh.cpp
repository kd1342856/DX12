#include "../../../Pch.h"
#include "../../../Framework/DirectX/Utility/Profiler.h"
#include "Mesh.h"
#include "../../Device/ResourceUploader.h"
#include "../../../Framework/DirectX/Utility/Thread.h"

// CreateGPU
// Main thread only: CPU data -> GPU buffer creation + upload command recording
// Begin()/End() is managed once by the outer GPUUploadQueue::Process()
// Do NOT call Begin()/End() inside this function
void Mesh::CreateGPU(GraphicsDevice* pDevice,
    const std::vector<MeshVertex>& vertices,
    const std::vector<MeshFace>&   faces,
    const Material&                material)
{
    assert(Thread::IsMainThread() && "Mesh::CreateGPU must be called on the main thread!");
    assert(!vertices.empty() && "Mesh::CreateGPU: vertices are empty");

    m_pDevice       = pDevice;
    m_material      = material;
    m_vertices      = vertices;
    m_faces         = faces;
    m_instanceCount = static_cast<UINT>(faces.size() * 3);

    // Create GPU buffers (empty, to be filled by upload)
    m_vertexBuffer.Create(
        pDevice,
        sizeof(MeshVertex),
        static_cast<UINT>(vertices.size()),
        nullptr);

    m_indexBuffer.Create(
        pDevice,
        static_cast<UINT>(faces.size() * 3),
        nullptr);

    // Record upload commands only (Begin/End managed by outer Process())
    pDevice->GetResourceUploader()->UploadVertexBuffer(
        &m_vertexBuffer,
        vertices.data(),
        sizeof(MeshVertex) * static_cast<UINT>(vertices.size()));

    pDevice->GetResourceUploader()->UploadIndexBuffer(
        &m_indexBuffer,
        faces.data(),
        sizeof(MeshFace) * static_cast<UINT>(faces.size()));

    // State is set to Ready by GPUUploadQueue::Process() after End()
}

void Mesh::DrawInstanced(UINT vertexCount) const
{
    OutputDebugStringA("DrawMesh\n");
    std::string name = m_material.Name.empty() ? "Unnamed Mesh" : m_material.Name;
    Profiler::Instance().AddDrawCall(name, 1);

    D3D12_VERTEX_BUFFER_VIEW vbView = m_vertexBuffer.GetView();
    m_pDevice->GetCmdList()->IASetVertexBuffers(0, 1, &vbView);

    D3D12_INDEX_BUFFER_VIEW ibView = m_indexBuffer.GetView();
    m_pDevice->GetCmdList()->IASetIndexBuffer(&ibView);

    m_pDevice->GetCmdList()->DrawIndexedInstanced(vertexCount, 1, 0, 0, 0);
}