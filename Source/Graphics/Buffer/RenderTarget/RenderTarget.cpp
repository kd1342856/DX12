#include "RenderTarget.h"
#include "../../Device/GraphicsDevice.h"
#include "../../Descriptors/Heap/RTVHeap/RTVHeap.h"
#include "../../Descriptors/Heap/CBVSRVUAVHeap/CBVSRVUAVHeap.h"
#include "../../Descriptors/Heap/DSVHeap/DSVHeap.h"

bool RenderTarget::Create(int width, int height)
{
	m_pGraphicsDevice = &GraphicsDevice::Instance();

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

	auto hr = m_pGraphicsDevice->GetDevice()->CreateCommittedResource(
		&heapProp, D3D12_HEAP_FLAG_NONE, &resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clearValue, IID_PPV_ARGS(&m_pBuffer));

	if (FAILED(hr)) return false;

	// RTVì¬
	m_rtvIndex = m_pGraphicsDevice->GetRTVHeap()->CreateRTV(m_pBuffer.Get());

	// SRVì¬
	m_srvIndex = m_pGraphicsDevice->GetCBVSRVUAVHeap()->CreateSRV(m_pBuffer.Get());
	m_imGuiSrvIndex = m_pGraphicsDevice->AllocateImGuiSRV(m_pBuffer.Get());

	// DepthStencilì¬
	D3D12_RESOURCE_DESC depthResDesc = resDesc;
	depthResDesc.Format = DXGI_FORMAT_D32_FLOAT;
	depthResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE depthClearValue = {};
	depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;
	depthClearValue.DepthStencil.Depth = 1.0f;

	hr = m_pGraphicsDevice->GetDevice()->CreateCommittedResource(
		&heapProp, D3D12_HEAP_FLAG_NONE, &depthResDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthClearValue, IID_PPV_ARGS(&m_pDepthBuffer));

	if (FAILED(hr)) return false;

	// DSVì¬
	m_dsvIndex = m_pGraphicsDevice->GetDSVHeap()->CreateDSV(m_pDepthBuffer.Get(), DXGI_FORMAT_D32_FLOAT);

	return true;
}

void RenderTarget::Clear(float r, float g, float b, float a)
{
	auto rtvH = m_pGraphicsDevice->GetRTVHeap()->GetCPUHandle(m_rtvIndex);
	auto dsvH = m_pGraphicsDevice->GetDSVHeap()->GetCPUHandle(m_dsvIndex);

	float clearColor[] = { r, g, b, a };
	m_pGraphicsDevice->GetCmdList()->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);
	m_pGraphicsDevice->GetCmdList()->ClearDepthStencilView(dsvH, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
}
