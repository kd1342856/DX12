#include "../../../Pch.h"


bool DepthStencil::Create(GraphicsDevice* pGraphicsDevice, const Math::Vector2& resolution, DepthStencilFormat format, bool bCreateSRV)
{
	m_device = pGraphicsDevice;
	m_format = format;
	m_hasSRV = bCreateSRV;
	if (!CreateResource(resolution))
	{
		return false;
	}

	// ディスクリプタはここで一度だけ確保し、Resize()では同じ番号に書き直す
	m_dsvNumber = m_device->GetDescriptorHeapManager()->GetDSVAllocator()->Allocate();
	if (bCreateSRV)
	{
		m_srvNumber = static_cast<int>(m_device->GetDescriptorHeapManager()->GetCBVSRVUAVAllocator()->Allocate());
	}
	WriteDescriptors();
	return true;
}

bool DepthStencil::Resize(const Math::Vector2& resolution)
{
	// 呼び出し側でGPUの処理が終わるのを待ってから呼ぶので、古いリソースはすぐに解放してよい
	m_device->GetResourceStateTracker()->RemoveResourceState(m_resource.Get());
	m_resource.Reset();
	if (!CreateResource(resolution))
	{
		return false;
	}
	WriteDescriptors();
	return true;
}

bool DepthStencil::CreateResource(const Math::Vector2& resolution)
{
	// 深度バッファ作成
	D3D12_HEAP_PROPERTIES heapProp = {};
	heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resDesc.Width = static_cast<UINT>(resolution.x);
	resDesc.Height = static_cast<UINT>(resolution.y);
	resDesc.DepthOrArraySize = 1;
	resDesc.Format = static_cast<DXGI_FORMAT>(m_format);
	resDesc.SampleDesc.Count = 1;
	resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	// 深度バッファのフォーマットと深度クリア値の設定
	D3D12_CLEAR_VALUE depthClearValue = {};
	depthClearValue.DepthStencil.Depth = 1.0f;

	switch (m_format)
	{
	case DepthStencilFormat::DepthLowQuality:
		depthClearValue.Format = DXGI_FORMAT_D16_UNORM;
		resDesc.Format = m_hasSRV ? DXGI_FORMAT_R16_TYPELESS : DXGI_FORMAT_D16_UNORM;
		break;
	case DepthStencilFormat::DepthHighQuality:
		depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;
		resDesc.Format = m_hasSRV ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_D32_FLOAT;
		break;
	case DepthStencilFormat::DepthHighQualityAndStencil:
		depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		resDesc.Format = m_hasSRV ? DXGI_FORMAT_R24G8_TYPELESS : DXGI_FORMAT_D24_UNORM_S8_UINT;
		break;
	default:
		break;
	}
	m_dsvFormat = depthClearValue.Format;

	// 実際に深度バッファを作成
	auto hr = m_device->GetDevice()->CreateCommittedResource(
		&heapProp, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClearValue, IID_PPV_ARGS(&m_resource));

	if (FAILED(hr))
	{
		assert(0 && "深度ステンシルバッファの生成に失敗しました");
		return false;
	}

	// 初期状態をトラッカーに登録
	m_device->GetResourceStateTracker()->AddResourceState(m_resource.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
	return true;
}

void DepthStencil::WriteDescriptors()
{
	auto* pHeapManager = m_device->GetDescriptorHeapManager();

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = m_dsvFormat;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	m_device->GetDevice()->CreateDepthStencilView(m_resource.Get(), &dsvDesc, pHeapManager->GetDSVAllocator()->GetCPUHandle(m_dsvNumber));

	if (m_hasSRV)
	{
		// TYPELESSの深度を、色のフォーマットとして読む
		DXGI_FORMAT srvFormat = DXGI_FORMAT_R32_FLOAT;
		if (m_format == DepthStencilFormat::DepthLowQuality)
		{
			srvFormat = DXGI_FORMAT_R16_UNORM;
		}
		else if (m_format == DepthStencilFormat::DepthHighQualityAndStencil)
		{
			srvFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		}
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = srvFormat;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		m_device->GetDevice()->CreateShaderResourceView(m_resource.Get(), &srvDesc, pHeapManager->GetCBVSRVUAVAllocator()->GetCPUHandle(m_srvNumber));
	}
}

void DepthStencil::ClearBuffer()
{
	m_device->GetCmdList()->ClearDepthStencilView(
		m_device->GetDescriptorHeapManager()->GetDSVAllocator()->GetCPUHandle(m_dsvNumber), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
}

