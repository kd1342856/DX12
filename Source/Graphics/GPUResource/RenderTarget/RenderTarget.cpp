#include "../../../Pch.h"
#include "RenderTarget.h"
bool RenderTarget::Create(int width, int height)
{
	m_width = width;
	m_height = height;
	m_device = &GraphicsDevice::Instance();
	D3D12_HEAP_PROPERTIES heapProp = {};
	heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resDesc.Width = width;
	resDesc.Height = height;
	resDesc.DepthOrArraySize = 1;
	resDesc.MipLevels = 1;
	resDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	resDesc.SampleDesc.Count = 1;
	resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	clearValue.Color[0] = 0.2f;
	clearValue.Color[1] = 0.2f;
	clearValue.Color[2] = 1.0f;
	clearValue.Color[3] = 1.0f;
	auto hr = m_device->GetDevice()->CreateCommittedResource(
		&heapProp, D3D12_HEAP_FLAG_NONE, &resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clearValue, IID_PPV_ARGS(&m_resource));
	if (FAILED(hr)) return false;
	// RTV??
	m_rtvIndex = m_device->CreateRTV(m_resource.Get());
	// SRV??
	m_srvIndex = m_device->CreateSRV(m_resource.Get());
	m_imGuiSrvIndex = m_device->AllocateImGuiSRV(m_resource.Get());
	// Actually create the SRV in the ImGui SRV Heap
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	
	auto handle = m_device->GetImGuiSRVGPUHandle(m_imGuiSrvIndex);
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_device->GetImGuiSRVHeap()->GetCPUDescriptorHandleForHeapStart();
	cpuHandle.ptr += m_device->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * m_imGuiSrvIndex;
	m_device->GetDevice()->CreateShaderResourceView(m_resource.Get(), &srvDesc, cpuHandle);
	// DepthStencil??
	D3D12_RESOURCE_DESC depthResDesc = resDesc;
	depthResDesc.Format = DXGI_FORMAT_D32_FLOAT;
	depthResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	D3D12_CLEAR_VALUE depthClearValue = {};
	depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;
	depthClearValue.DepthStencil.Depth = 1.0f;
	hr = m_device->GetDevice()->CreateCommittedResource(
		&heapProp, D3D12_HEAP_FLAG_NONE, &depthResDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthClearValue, IID_PPV_ARGS(&m_pDepthBuffer));
	if (FAILED(hr)) return false;
	// DSV??
	m_dsvIndex = m_device->CreateDSV(m_pDepthBuffer.Get(), DXGI_FORMAT_D32_FLOAT);
	return true;
}
void RenderTarget::Clear(float r, float g, float b, float a)
{
	auto rtvH = m_device->GetDescriptorHeapManager()->GetRTVAllocator()->GetCPUHandle(m_rtvIndex);
	auto dsvH = m_device->GetDescriptorHeapManager()->GetDSVAllocator()->GetCPUHandle(m_dsvIndex);
	float clearColor[] = { r, g, b, a };
	m_device->GetCmdList()->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);
	m_device->GetCmdList()->ClearDepthStencilView(dsvH, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
}


