#include "../../Pch.h"
#include "RaytracingScene.h"
#include "../Geometry/Mesh/Mesh.h"
#include "../../Framework/Manager/Asset/TextureManager.h"
#include "../../Framework/DirectX/Utility/Profiler.h"
#include "../Shader/ComputeShader/ComputeShader.h"

namespace
{
    constexpr uint32_t kInvalidIndex = 0xFFFFFFFFu;

    UINT64 AlignUp(UINT64 value, UINT64 alignment)
    {
        return (value + alignment - 1) & ~(alignment - 1);
    }

    D3D12_RESOURCE_DESC MakeBufferDesc(UINT64 size, D3D12_RESOURCE_FLAGS flags)
    {
        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = size;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        desc.Flags = flags;
        return desc;
    }

    uint32_t GetTextureIndex(const MaterialTexture& materialTexture)
    {
        if (!materialTexture.valid)
        {
            return kInvalidIndex;
        }
        Texture* pTexture = TextureManager::Instance().Get(materialTexture.handle);
        if (!pTexture || !pTexture->IsReady() || pTexture->GetSRVNumber() < 0)
        {
            return kInvalidIndex;
        }
        return static_cast<uint32_t>(pTexture->GetSRVNumber());
    }
}

void RaytracingScene::Initialize()
{
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
    auto* pDevice = GraphicsDevice::Instance().GetDevice();
    if (FAILED(pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5))) ||
        options5.RaytracingTier < D3D12_RAYTRACING_TIER_1_1 || !ComputeShader::IsSupported())
    {
        Logger::Instance().AddLog(Logger::LogLevel::Warning, u8"[RaytracingScene] DXR 1.1に対応していないため、GIを無効にします");
        return;
    }
    // TLASとGeometryInfoのSRVは、フレームごとに番号を決めておき、作り直すたびに同じ番号へ書く
    auto* pAllocator = GraphicsDevice::Instance().GetDescriptorHeapManager()->GetCBVSRVUAVAllocator();
    for (FrameResources& frame : m_frames)
    {
        frame.tlasSrvIndex = static_cast<int>(pAllocator->Allocate());
        frame.geometrySrvIndex = static_cast<int>(pAllocator->Allocate());
    }
    m_isSupported = true;
}

void RaytracingScene::Clear()
{
    auto& device = GraphicsDevice::Instance();
    const uint64_t fenceValue = device.GetQueueManager()->GetGraphicsQueue()->GetFenceValue();
    for (auto& [pMesh, bottomLevel] : m_bottomLevels)
    {
        if (bottomLevel.cpBuffer)
        {
            device.DeferDeleteResource(std::move(bottomLevel.cpBuffer), fenceValue);
        }
        // SRVの番号は作り直したBLASで使い回せないので、ヒープへ返す
        auto* pAllocator = device.GetDescriptorHeapManager()->GetCBVSRVUAVAllocator();
        pAllocator->Free(static_cast<UINT>(bottomLevel.vertexSrvIndex));
        pAllocator->Free(static_cast<UINT>(bottomLevel.indexSrvIndex));
    }
    m_bottomLevels.clear();
    m_pendingBuilds.clear();
    m_hasTLAS = false;
}

bool RaytracingScene::EnsureBuffer(Microsoft::WRL::ComPtr<ID3D12Resource>& cpBuffer, UINT64& capacity, UINT64 size, D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES state, const wchar_t* pName)
{
    if (cpBuffer && capacity >= size)
    {
        return true;
    }
    auto& device = GraphicsDevice::Instance();
    if (cpBuffer)
    {
        device.DeferDeleteResource(std::move(cpBuffer), device.GetQueueManager()->GetGraphicsQueue()->GetFenceValue());
    }
    // 足りなくなるたびに作り直さないよう、少し大きめに取る
    capacity = AlignUp(size + size / 2, 65536);
    D3D12_HEAP_PROPERTIES heapProperties = {};
    heapProperties.Type = heapType;
    const D3D12_RESOURCE_FLAGS flags = (heapType == D3D12_HEAP_TYPE_DEFAULT) ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;
    const D3D12_RESOURCE_DESC desc = MakeBufferDesc(capacity, flags);
    if (FAILED(device.GetDevice()->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &desc, state, nullptr, IID_PPV_ARGS(&cpBuffer))))
    {
        Logger::Instance().AddLog(Logger::LogLevel::Error, u8"[RaytracingScene] バッファを作れませんでした (%llu bytes)", capacity);
        capacity = 0;
        return false;
    }
    cpBuffer->SetName(pName);
    return true;
}

void RaytracingScene::BeginFrame()
{
    m_instanceDescs.clear();
    m_geometryInfos.clear();
    m_pendingBuilds.clear();
    m_frameSlot = (m_frameSlot + 1) % kFrameCount;
}

RaytracingScene::BottomLevel* RaytracingScene::FindOrCreateBottomLevel(const Mesh& mesh)
{
    const D3D12_GPU_VIRTUAL_ADDRESS vertexAddress = mesh.GetVertexBuffer().GetGPUVirtualAddress();
    auto it = m_bottomLevels.find(&mesh);
    if (it != m_bottomLevels.end() && it->second.vertexAddress == vertexAddress)
    {
        return &it->second;
    }
    if (static_cast<int>(m_pendingBuilds.size()) >= kMaxBottomLevelBuildsPerFrame)
    {
        return nullptr;
    }

    // 同じアドレスのメッシュが作り直されていたら、古いBLASを捨てる
    BottomLevel& bottomLevel = m_bottomLevels[&mesh];
    if (bottomLevel.cpBuffer)
    {
        auto& device = GraphicsDevice::Instance();
        device.DeferDeleteResource(std::move(bottomLevel.cpBuffer), device.GetQueueManager()->GetGraphicsQueue()->GetFenceValue());
    }
    if (bottomLevel.vertexSrvIndex == -1)
    {
        bottomLevel.vertexSrvIndex = static_cast<int>(GraphicsDevice::Instance().GetDescriptorHeapManager()->GetCBVSRVUAVAllocator()->Allocate());
        bottomLevel.indexSrvIndex = static_cast<int>(GraphicsDevice::Instance().GetDescriptorHeapManager()->GetCBVSRVUAVAllocator()->Allocate());
    }
    bottomLevel.vertexAddress = vertexAddress;
    bottomLevel.isBuilt = false;

    // シェーダーが頂点とインデックスを読むためのSRV(番号は使い回す)
    auto& device = GraphicsDevice::Instance();
    auto* pAllocator = device.GetDescriptorHeapManager()->GetCBVSRVUAVAllocator();
    const UINT64 vertexBytes = static_cast<UINT64>(mesh.GetVertices().size()) * sizeof(MeshVertex);
    const UINT64 indexBytes = static_cast<UINT64>(mesh.GetInstanceCount()) * sizeof(uint32_t);
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
    srvDesc.Buffer.NumElements = static_cast<UINT>(vertexBytes / 4);
    device.GetDevice()->CreateShaderResourceView(mesh.GetVertexBuffer().GetResource(), &srvDesc, pAllocator->GetCPUHandle(bottomLevel.vertexSrvIndex));
    srvDesc.Buffer.NumElements = static_cast<UINT>(indexBytes / 4);
    device.GetDevice()->CreateShaderResourceView(mesh.GetIndexBuffer().GetResource(), &srvDesc, pAllocator->GetCPUHandle(bottomLevel.indexSrvIndex));

    m_pendingBuilds.emplace_back(&mesh, &bottomLevel);
    return &bottomLevel;
}

void RaytracingScene::AddInstance(const Mesh& mesh, const Material& material, const Math::Matrix& world)
{
    if (!m_isSupported || !mesh.IsReady() || mesh.GetInstanceCount() == 0)
    {
        return;
    }
    BottomLevel* pBottomLevel = FindOrCreateBottomLevel(mesh);
    if (!pBottomLevel)
    {
        return;
    }

    D3D12_RAYTRACING_INSTANCE_DESC instance = {};
    // DXRの変換は列ベクトルの3x4。行ベクトルのワールド行列の上3列を転置して入れる
    for (int row = 0; row < 3; ++row)
    {
        instance.Transform[row][0] = world.m[0][row];
        instance.Transform[row][1] = world.m[1][row];
        instance.Transform[row][2] = world.m[2][row];
        instance.Transform[row][3] = world.m[3][row];
    }
    instance.InstanceID = static_cast<UINT>(m_geometryInfos.size());
    instance.InstanceMask = 0xFF;
    // 両面とも当たるようにする(裏面かどうかはシェーダーで頂点の法線から判定する)
    instance.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE | D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_OPAQUE;
    // BLASのアドレスはBuildで作った後に決まるので、ここでは対応するBLASを覚えておき、Buildで埋める
    instance.AccelerationStructure = reinterpret_cast<D3D12_GPU_VIRTUAL_ADDRESS>(pBottomLevel);
    m_instanceDescs.push_back(instance);

    GeometryInfo info = {};
    info.vertexBufferIndex = static_cast<uint32_t>(pBottomLevel->vertexSrvIndex);
    info.indexBufferIndex = static_cast<uint32_t>(pBottomLevel->indexSrvIndex);
    info.baseColorTextureIndex = GetTextureIndex(material.BaseColor);
    info.emissiveTextureIndex = GetTextureIndex(material.Emissive);
    info.baseColorFactor = material.Constants.baseColorFactor;
    info.emissive = Math::Vector3(material.Constants.emissiveFactorX, material.Constants.emissiveFactorY, material.Constants.emissiveFactorZ) * material.Constants.emissiveStrength;
    m_geometryInfos.push_back(info);
}

void RaytracingScene::Build()
{
    m_hasTLAS = false;
    if (!m_isSupported || m_instanceDescs.empty())
    {
        return;
    }
    auto& device = GraphicsDevice::Instance();
    auto* pDevice = device.GetDevice();
    auto* pCmdList = device.GetCmdList();
    PROFILE_GPU_SCOPE(pCmdList, "BuildAccelerationStructures");

    // 1. このフレームで新しく作るBLASの大きさを調べ、まとめて1つの作業用バッファから切り出す
    std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometryDescs(m_pendingBuilds.size());
    std::vector<D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS> bottomInputs(m_pendingBuilds.size());
    std::vector<UINT64> scratchOffsets(m_pendingBuilds.size());
    UINT64 scratchTotal = 0;
    for (size_t i = 0; i < m_pendingBuilds.size(); ++i)
    {
        const Mesh& mesh = *m_pendingBuilds[i].first;
        BottomLevel& bottomLevel = *m_pendingBuilds[i].second;
        D3D12_RAYTRACING_GEOMETRY_DESC& geometry = geometryDescs[i];
        geometry.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        geometry.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
        geometry.Triangles.VertexBuffer.StartAddress = mesh.GetVertexBuffer().GetGPUVirtualAddress();
        geometry.Triangles.VertexBuffer.StrideInBytes = sizeof(MeshVertex);
        geometry.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
        geometry.Triangles.VertexCount = static_cast<UINT>(mesh.GetVertices().size());
        geometry.Triangles.IndexBuffer = mesh.GetIndexBuffer().GetGPUVirtualAddress();
        geometry.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
        geometry.Triangles.IndexCount = mesh.GetInstanceCount();

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& inputs = bottomInputs[i];
        inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
        inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
        inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        inputs.NumDescs = 1;
        inputs.pGeometryDescs = &geometry;

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild = {};
        pDevice->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuild);
        UINT64 resultCapacity = 0;
        if (!EnsureBuffer(bottomLevel.cpBuffer, resultCapacity, prebuild.ResultDataMaxSizeInBytes, D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, L"BLAS"))
        {
            continue;
        }
        scratchOffsets[i] = scratchTotal;
        scratchTotal = AlignUp(scratchTotal + prebuild.ScratchDataSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);
    }

    // 2. TLASの入力(インスタンス)。BLASのアドレスを埋めてアップロードのバッファへ写す
    FrameResources& frame = m_frames[m_frameSlot];
    const UINT instanceCount = static_cast<UINT>(m_instanceDescs.size());
    UINT64 instanceCapacity = frame.instanceCapacity;
    UINT64 geometryCapacity = frame.instanceCapacity;
    if (!EnsureBuffer(frame.cpInstanceBuffer, instanceCapacity, sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * instanceCount, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, L"TLASInstances") ||
        !EnsureBuffer(frame.cpGeometryBuffer, geometryCapacity, sizeof(GeometryInfo) * instanceCount, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, L"RaytracingGeometry"))
    {
        return;
    }
    frame.instanceCapacity = static_cast<UINT>((std::min)(instanceCapacity / sizeof(D3D12_RAYTRACING_INSTANCE_DESC), geometryCapacity / sizeof(GeometryInfo)));

    D3D12_RAYTRACING_INSTANCE_DESC* pInstances = nullptr;
    frame.cpInstanceBuffer->Map(0, nullptr, reinterpret_cast<void**>(&pInstances));
    UINT validCount = 0;
    for (UINT i = 0; i < instanceCount; ++i)
    {
        D3D12_RAYTRACING_INSTANCE_DESC instance = m_instanceDescs[i];
        const BottomLevel* pBottomLevel = reinterpret_cast<const BottomLevel*>(instance.AccelerationStructure);
        if (!pBottomLevel->cpBuffer)
        {
            continue;
        }
        instance.AccelerationStructure = pBottomLevel->cpBuffer->GetGPUVirtualAddress();
        pInstances[validCount++] = instance;
    }
    frame.cpInstanceBuffer->Unmap(0, nullptr);

    void* pGeometry = nullptr;
    frame.cpGeometryBuffer->Map(0, nullptr, &pGeometry);
    std::memcpy(pGeometry, m_geometryInfos.data(), sizeof(GeometryInfo) * m_geometryInfos.size());
    frame.cpGeometryBuffer->Unmap(0, nullptr);

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS topInputs = {};
    topInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    topInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    topInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    topInputs.NumDescs = validCount;
    topInputs.InstanceDescs = frame.cpInstanceBuffer->GetGPUVirtualAddress();
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topPrebuild = {};
    pDevice->GetRaytracingAccelerationStructurePrebuildInfo(&topInputs, &topPrebuild);
    const UINT64 topScratchOffset = scratchTotal;
    scratchTotal = AlignUp(scratchTotal + topPrebuild.ScratchDataSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);

    // 作業用バッファ。バッファは常にCOMMONで作られ、加速構造を作るときにUAVへ暗黙に昇格する
    if (!EnsureBuffer(m_cpScratch, m_scratchSize, scratchTotal, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, L"AccelerationStructureScratch"))
    {
        return;
    }
    const UINT64 previousTLASSize = frame.tlasSize;
    if (!EnsureBuffer(frame.cpTLAS, frame.tlasSize, topPrebuild.ResultDataMaxSizeInBytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, L"TLAS"))
    {
        return;
    }
    // TLASを作り直したら、SRVも書き直す(このフレームの番号なので、GPUは使っていない)
    if (frame.tlasSize != previousTLASSize)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
        srvDesc.RaytracingAccelerationStructure.Location = frame.cpTLAS->GetGPUVirtualAddress();
        pDevice->CreateShaderResourceView(nullptr, &srvDesc, device.GetDescriptorHeapManager()->GetCBVSRVUAVAllocator()->GetCPUHandle(frame.tlasSrvIndex));
    }
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Buffer.NumElements = static_cast<UINT>(m_geometryInfos.size());
        srvDesc.Buffer.StructureByteStride = sizeof(GeometryInfo);
        pDevice->CreateShaderResourceView(frame.cpGeometryBuffer.Get(), &srvDesc, device.GetDescriptorHeapManager()->GetCBVSRVUAVAllocator()->GetCPUHandle(frame.geometrySrvIndex));
    }

    // 3. 前のフレームが作業用バッファを使い終わるのを待ってから(UAVのバリア)、BLAS、TLASの順に作る
    D3D12_RESOURCE_BARRIER uavBarrier = {};
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.UAV.pResource = nullptr;
    device.GetContextManager()->GetGraphicsContext()->FlushResourceBarriers();
    pCmdList->ResourceBarrier(1, &uavBarrier);

    for (size_t i = 0; i < m_pendingBuilds.size(); ++i)
    {
        BottomLevel& bottomLevel = *m_pendingBuilds[i].second;
        if (!bottomLevel.cpBuffer)
        {
            continue;
        }
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
        buildDesc.Inputs = bottomInputs[i];
        buildDesc.Inputs.pGeometryDescs = &geometryDescs[i];
        buildDesc.DestAccelerationStructureData = bottomLevel.cpBuffer->GetGPUVirtualAddress();
        buildDesc.ScratchAccelerationStructureData = m_cpScratch->GetGPUVirtualAddress() + scratchOffsets[i];
        pCmdList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);
        bottomLevel.isBuilt = true;
    }
    // BLASを書き終えてからTLASを作る
    pCmdList->ResourceBarrier(1, &uavBarrier);

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topBuild = {};
    topBuild.Inputs = topInputs;
    topBuild.DestAccelerationStructureData = frame.cpTLAS->GetGPUVirtualAddress();
    topBuild.ScratchAccelerationStructureData = m_cpScratch->GetGPUVirtualAddress() + topScratchOffset;
    pCmdList->BuildRaytracingAccelerationStructure(&topBuild, 0, nullptr);
    // TLASを書き終えてから、レイトレースで読む
    pCmdList->ResourceBarrier(1, &uavBarrier);

    m_hasTLAS = (validCount > 0);
}
