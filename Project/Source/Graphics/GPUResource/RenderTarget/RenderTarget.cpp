#include "../../../Pch.h"
#include "RenderTarget.h"
bool RenderTarget::Create(int width, int height, DXGI_FORMAT format)
{
	m_device = &GraphicsDevice::Instance();
	m_format = format;
	if (!CreateResources(width, height))
	{
		return false;
	}

	// ディスクリプタはここで一度だけ確保し、Resize()では同じ番号に書き直す
	// (確保し直すとリサイズのたびにヒープを使い切っていくため)
	m_rtvIndex = m_device->GetDescriptorHeapManager()->GetRTVAllocator()->Allocate();
	m_srvIndex = m_device->GetDescriptorHeapManager()->GetCBVSRVUAVAllocator()->Allocate();
	m_imGuiSrvIndex = m_device->AllocateImGuiSRVIndex();
	m_dsvIndex = m_device->GetDescriptorHeapManager()->GetDSVAllocator()->Allocate();
	m_depthSrvIndex = m_device->GetDescriptorHeapManager()->GetCBVSRVUAVAllocator()->Allocate();
	WriteDescriptors();
	return true;
}

bool RenderTarget::Resize(int width, int height)
{
	if (width == m_width && height == m_height)
	{
		return true;
	}
	// 呼び出し側でGPUの処理が終わるのを待ってから呼ぶので、古いリソースはすぐに解放してよい
	auto* pTracker = m_device->GetResourceStateTracker();
	pTracker->RemoveResourceState(m_resource.Get());
	pTracker->RemoveResourceState(m_pDepthBuffer.Get());
	m_resource.Reset();
	m_pDepthBuffer.Reset();
	if (!CreateResources(width, height))
	{
		return false;
	}
	WriteDescriptors();
	return true;
}

bool RenderTarget::CreateResources(int width, int height)
{
	m_width = width;
	m_height = height;
	D3D12_HEAP_PROPERTIES heapProp = {};
	heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resDesc.Width = width;
	resDesc.Height = height;
	resDesc.DepthOrArraySize = 1;
	resDesc.MipLevels = 1;
	resDesc.Format = m_format;
	resDesc.SampleDesc.Count = 1;
	resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = m_format;
	clearValue.Color[0] = 0.2f;
	clearValue.Color[1] = 0.2f;
	clearValue.Color[2] = 1.0f;
	clearValue.Color[3] = 1.0f;
	auto hr = m_device->GetDevice()->CreateCommittedResource(
		&heapProp, D3D12_HEAP_FLAG_NONE, &resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clearValue, IID_PPV_ARGS(&m_resource));
	if (FAILED(hr)) return false;
	// DepthStencil。SRVからも読めるように(DOF/SSAO/SSR等)、DSVと同じ理由でTypelessにしておき、
	// DSV側はD32_FLOAT、SRV側はR32_FLOATとしてそれぞれ見る(DepthStencilクラスと同じ手法)。
	D3D12_RESOURCE_DESC depthResDesc = resDesc;
	depthResDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	depthResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	D3D12_CLEAR_VALUE depthClearValue = {};
	depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;
	depthClearValue.DepthStencil.Depth = 1.0f;
	hr = m_device->GetDevice()->CreateCommittedResource(
		&heapProp, D3D12_HEAP_FLAG_NONE, &depthResDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthClearValue, IID_PPV_ARGS(&m_pDepthBuffer));
	if (FAILED(hr)) return false;

	auto* pTracker = m_device->GetResourceStateTracker();
	pTracker->AddResourceState(m_resource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	pTracker->AddResourceState(m_pDepthBuffer.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
	return true;
}

void RenderTarget::WriteDescriptors()
{
	ID3D12Device* pDevice = m_device->GetDevice();
	auto* pHeapManager = m_device->GetDescriptorHeapManager();

	pDevice->CreateRenderTargetView(m_resource.Get(), nullptr, pHeapManager->GetRTVAllocator()->GetCPUHandle(m_rtvIndex));

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = m_format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	pDevice->CreateShaderResourceView(m_resource.Get(), &srvDesc, pHeapManager->GetCBVSRVUAVAllocator()->GetCPUHandle(m_srvIndex));

	// ImGuiのSRVヒープにも同じSRVを作る(エディターの画面表示用)
	D3D12_CPU_DESCRIPTOR_HANDLE imGuiHandle = m_device->GetImGuiSRVHeap()->GetCPUDescriptorHandleForHeapStart();
	imGuiHandle.ptr += static_cast<SIZE_T>(pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)) * m_imGuiSrvIndex;
	pDevice->CreateShaderResourceView(m_resource.Get(), &srvDesc, imGuiHandle);

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	pDevice->CreateDepthStencilView(m_pDepthBuffer.Get(), &dsvDesc, pHeapManager->GetDSVAllocator()->GetCPUHandle(m_dsvIndex));

	D3D12_SHADER_RESOURCE_VIEW_DESC depthSrvDesc = {};
	depthSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	depthSrvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	depthSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	depthSrvDesc.Texture2D.MipLevels = 1;
	pDevice->CreateShaderResourceView(m_pDepthBuffer.Get(), &depthSrvDesc, pHeapManager->GetCBVSRVUAVAllocator()->GetCPUHandle(m_depthSrvIndex));
}
void RenderTarget::ClearColor(float r, float g, float b, float a)
{
	auto rtvH = m_device->GetDescriptorHeapManager()->GetRTVAllocator()->GetCPUHandle(m_rtvIndex);
	float clearColor[] = { r, g, b, a };
	m_device->GetCmdList()->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);
}

void RenderTarget::Clear(float r, float g, float b, float a)
{
	auto rtvH = m_device->GetDescriptorHeapManager()->GetRTVAllocator()->GetCPUHandle(m_rtvIndex);
	auto dsvH = m_device->GetDescriptorHeapManager()->GetDSVAllocator()->GetCPUHandle(m_dsvIndex);
	float clearColor[] = { r, g, b, a };
	m_device->GetCmdList()->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);
	m_device->GetCmdList()->ClearDepthStencilView(dsvH, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
}


