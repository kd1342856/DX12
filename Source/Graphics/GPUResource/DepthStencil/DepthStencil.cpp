#include "../../../Pch.h"


bool DepthStencil::Create(GraphicsDevice* pGraphicsDevice, const Math::Vector2& resolution, DepthStencilFormat format, bool bCreateSRV)
{
	m_device = pGraphicsDevice;

	// 深度バッファ作成
	D3D12_HEAP_PROPERTIES heapProp = {};
	heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resDesc.Width = static_cast<UINT>(resolution.x);
	resDesc.Height = static_cast<UINT>(resolution.y);
	resDesc.DepthOrArraySize = 1;
	resDesc.Format = static_cast<DXGI_FORMAT>(format);
	resDesc.SampleDesc.Count = 1;
	resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	// 深度バッファのフォーマットと深度クリア値の設定
	D3D12_CLEAR_VALUE depthClearValue = {};
	depthClearValue.DepthStencil.Depth = 1.0f;

	switch (format)
	{
	case DepthStencilFormat::DepthLowQuality:
		depthClearValue.Format = DXGI_FORMAT_D16_UNORM;
		resDesc.Format = bCreateSRV ? DXGI_FORMAT_R16_TYPELESS : DXGI_FORMAT_D16_UNORM;
		break;
	case DepthStencilFormat::DepthHighQuality:
		depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;
		resDesc.Format = bCreateSRV ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_D32_FLOAT;
		break;
	case DepthStencilFormat::DepthHighQualityAndStencil:
		depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		resDesc.Format = bCreateSRV ? DXGI_FORMAT_R24G8_TYPELESS : DXGI_FORMAT_D24_UNORM_S8_UINT;
		break;
	default:
		break;
	}
	// 実際に深度バッファを作成
	auto hr = m_device->GetDevice()->CreateCommittedResource(
		&heapProp, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClearValue, IID_PPV_ARGS(&m_resource));

	if (FAILED(hr))
	{
		assert(0 && "深度ステンシルバッファの生成に失敗しました");
		return false;
	}

	// DSVを作成
	m_dsvNumber = m_device->CreateDSV(m_resource.Get(), depthClearValue.Format);

	if (bCreateSRV)
	{
		m_srvNumber = m_device->CreateSRV(m_resource.Get());
	}

	// 初期状態をトラッカーに登録
	m_device->GetResourceStateTracker()->AddResourceState(m_resource.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE);

	return true;
}

void DepthStencil::ClearBuffer()
{
	m_device->GetCmdList()->ClearDepthStencilView(
		m_device->GetDescriptorHeapManager()->GetDSVAllocator()->GetCPUHandle(m_dsvNumber), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
}

