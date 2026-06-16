#include "../Buffer/RenderTarget/RenderTarget.h"
#include "GraphicsDevice.h"

bool GraphicsDevice::Init(HWND  hWnd, int w, int h)
{
	if (!CreateFactory())
	{
		assert(0 && "繝輔ぃ繧ｯ繝医Μ縺ｮ菴懈・縺ｫ螟ｱ謨励＠縺ｾ縺励◆");
		return false;
	}
#ifdef _DEBUG
	EnableDebugLayer();
#endif
	if (!CreateDevice())
	{
		assert(0 && "繝・ヰ繧､繧ｹ縺ｮ菴懈・縺ｫ螟ｱ謨励＠縺ｾ縺励◆");
		return false;
	}
	if (!CreateCommandList())
	{
		assert(0 && "繧ｳ繝槭Φ繝峨Μ繧ｹ繝医・菴懈・縺ｫ螟ｱ謨励＠縺ｾ縺励◆");
		return false;
	}
	if (!CreateSwapChain(hWnd, w, h))
	{
		assert(0 && "繧ｹ繝ｯ繝・・繝√ぉ繝ｼ繝ｳ縺ｮ菴懈・縺ｫ螟ｱ謨励＠縺ｾ縺励◆");
		return false;
	}
	m_pRTVHeap = std::make_unique<RTVHeap>();
	if (!m_pRTVHeap->Create(this, HeapType::RTV, 100))
	{
		assert(0 && "RTV繝偵・繝励・菴懈・縺ｫ螟ｱ謨励＠縺ｾ縺励◆");
		return false;
	}
	m_upCBVSRVUAVHeap	= std::make_unique<CBVSRVUAVHeap>();
	if (!m_upCBVSRVUAVHeap->Create(this, HeapType::CBVSRVUAV, Math::Vector3(10000, 10000, 10000)))
	{
		assert(0 && "CBV/SRV/UAV繝偵・繝励・菴懈・縺ｫ螟ｱ謨励＠縺ｾ縺励◆");
		return false;
	}

	m_upCBufferAllocator = std::make_unique<CBufferAllocator>();
	m_upCBufferAllocator->Create(this, m_upCBVSRVUAVHeap.get());

	m_upDSVHeap = std::make_unique<DSVHeap>();
	if (!m_upDSVHeap->Create(this, HeapType::DSV, 100))
	{
		assert(0 && "DSV繝偵・繝励・菴懈・縺ｫ螟ｱ謨励＠縺ｾ縺励◆");
		return false;
	}
	m_upDepthStencil = std::make_unique<DepthStencil>();
	if(!m_upDepthStencil->Create(this, Math::Vector2(w, h)))
	{
		assert(0 && "豺ｱ蠎ｦ繧ｹ繝・Φ繧ｷ繝ｫ縺ｮ菴懈・縺ｫ螟ｱ謨励＠縺ｾ縺励◆");
		return false;
	}

	m_upShadowMap = std::make_unique<DepthStencil>();
	if(!m_upShadowMap->Create(this, Math::Vector2(4096, 4096), DepthStencilFormat::DepthHighQuality, true))
	{
		assert(0 && "繧ｷ繝｣繝峨え繝槭ャ繝励・菴懈・縺ｫ螟ｱ謨励＠縺ｾ縺励◆");
		return false;
	}

	if (!CreateSwapChainRTV())
	{
		assert(0 && "繧ｹ繝ｯ繝・・繝√ぉ繝ｼ繝ｳ縺ｮRTV菴懈・縺ｫ螟ｱ謨励＠縺ｾ縺励◆");
		return false;
	}
	if (!CreateFence())
	{
		assert(0 && "繝輔ぉ繝ｳ繧ｹ縺ｮ菴懈・縺ｫ螟ｱ謨励＠縺ｾ縺励◆");
		return false;
	}
	m_fenceEvent = CreateEvent(nullptr, false, false, nullptr);
	if (!m_fenceEvent)
	{
		assert(0 && "Event error");
		return false;
	}
	m_graphicsMemory = std::make_unique<DirectX::GraphicsMemory>(m_pDevice.Get());

	CreateDefaultTextures();
	if (!InitImGui())
	{
		assert(0 && "ImGui縺ｮ蛻晄悄蛹悶↓螟ｱ謨励＠縺ｾ縺励◆");
		return false;
	}
	return true;
}

void GraphicsDevice::EndFrame()
{
	auto bbIdx = m_pSwapChain->GetCurrentBackBufferIndex();
	RenderImGui();
	SetResourceBarrier(m_pSwapchainBuffers[bbIdx].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

	m_pCmdList->Close();
	ID3D12CommandList* cmdlists[] = { m_pCmdList.Get() };
	m_pCmdQueue->ExecuteCommandLists(1, cmdlists);

	//	繝輔Ξ繝ｼ繝縺ｮ霆｢蜃ｺ逡ｪ蜿ｷ繧定ｨ倬鹸
	const int frameIdx = m_frameIndex % kFrameCount;
	m_frames[frameIdx].fenceValue = SignalQueue();

	//	繧ｹ繝ｯ繝・・繝√ぉ繧､繝ｳ縺ｫ繝励Ξ繧ｼ繝ｳ繝茨ｼ磯√ｋ・・
	m_pSwapChain->Present(1, 0);
	m_graphicsMemory->Commit(m_pCmdQueue.Get());
	++m_frameIndex;
}

void GraphicsDevice::BeginFrame()
{
	//	Frame Begin
	const int frameIdx = m_frameIndex % kFrameCount;
	FrameContext& fr = m_frames[frameIdx];
	WaitForFence(fr.fenceValue);
	fr.allocator->Reset();
	m_pCmdList->Reset(fr.allocator.Get(), nullptr);

	auto bbIdx = m_pSwapChain->GetCurrentBackBufferIndex();
	SetResourceBarrier(m_pSwapchainBuffers[bbIdx].Get(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

	auto rtvH = m_pRTVHeap->GetCPUHandle(bbIdx);

	auto dsvH = m_upDSVHeap->GetCPUHandle(m_upDepthStencil->GetDSVNumber());

	m_pCmdList->OMSetRenderTargets(1, &rtvH, false, &dsvH);

	float clearColor[] = { 0.0f,0.0f,1.0f,1.0f };	//	髱・
	m_pCmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);

	m_upDepthStencil->ClearBuffer();
}

void GraphicsDevice::WaitForCommandQueue()
{
	WaitForFence(SignalQueue());
}

bool GraphicsDevice::CreateFactory()
{
	UINT flagsDXGI = 0;
	flagsDXGI |= DXGI_CREATE_FACTORY_DEBUG;
	auto result = CreateDXGIFactory2(flagsDXGI, IID_PPV_ARGS(m_pDxgiFactory.GetAddressOf()));
	if (FAILED(result))
	{
		return false;
	}
	return true;
}

bool GraphicsDevice::CreateDevice()
{
	ComPtr<IDXGIAdapter>				pSelectAdapter = nullptr;
	std::vector<ComPtr<IDXGIAdapter>>	pAdapters;
	std::vector<DXGI_ADAPTER_DESC>		descs;

	//	菴ｿ逕ｨ荳ｭPC縺ｫ縺ゅｋGOU繝峨Λ繧､繝舌・繧呈､懃ｴ｢縺励※縲√≠繧後・譬ｼ邏・
	for (UINT index = 0; 1; ++index)
	{
		pAdapters.push_back(nullptr);
		HRESULT ret = m_pDxgiFactory->EnumAdapters(index, &pAdapters[index]);

		if (ret == DXGI_ERROR_NOT_FOUND) { break; }

		descs.push_back({});
		pAdapters[index]->GetDesc(&descs[index]);
	}

	GPUTier gpuTier = GPUTier::Kind;

	for (int i = 0; i < descs.size(); ++i)
	{
		if (std::wstring(descs[i].Description).find(L"NVIDIA") != std::wstring::npos)
		{
			pSelectAdapter = pAdapters[i];
			break;
		}
		else if (std::wstring(descs[i].Description).find(L"Amd") != std::wstring::npos)
		{
			if (gpuTier > GPUTier::Amd)
			{
				pSelectAdapter = pAdapters[i];
				gpuTier = GPUTier::Amd;
			}
		}
		else if (std::wstring(descs[i].Description).find(L"Intel") != std::wstring::npos)
		{
			if (gpuTier > GPUTier::Intel)
			{
				pSelectAdapter = pAdapters[i];
				gpuTier = GPUTier::Intel;
			}
		}
		else if (std::wstring(descs[i].Description).find(L"Arm") != std::wstring::npos)
		{
			if (gpuTier > GPUTier::Arm)
			{
				pSelectAdapter = pAdapters[i];
				gpuTier = GPUTier::Arm;
			}
		}
		else if (std::wstring(descs[i].Description).find(L"Qualcomm") != std::wstring::npos)
		{
			if (gpuTier > GPUTier::Qualcomm)
			{
				pSelectAdapter = pAdapters[i];
				gpuTier = GPUTier::Qualcomm;
			}
		}
	}

	D3D_FEATURE_LEVEL levels[] =
	{
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};

	//	Direct3D繝・ヰ繧､繧ｹ縺ｮ蛻晄悄蛹・
	D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
	for (auto lv : levels)
	{
		if (D3D12CreateDevice(pSelectAdapter.Get(), lv, IID_PPV_ARGS(&m_pDevice)) == S_OK)
		{
			featureLevel = lv;
			break;	//逕滓・蜿ｯ閭ｽ縺ｪ繝舌・繧ｸ繝ｧ繝ｳ縺瑚ｦ九▽縺九▲縺溘ｉ繝ｫ繝ｼ繝玲遠縺｡蛻・ｊ
		}
	}
	return true;
}

bool GraphicsDevice::CreateCommandList()
{
	for (int i = 0; i < kFrameCount; ++i)
	{
		auto hr = m_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_frames[i].allocator));
		if (FAILED(hr))
		{
			return false;
		}
		m_frames[i].fenceValue = 0;
	}
	auto hr = m_pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_frames[0].allocator.Get(), nullptr, IID_PPV_ARGS(&m_pCmdList));
	if (FAILED(hr))
	{
		return false;
	}
	m_pCmdList->Close();

	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
	cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	//	繧ｭ繝･繝ｼ逕滓・
	hr = m_pDevice->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&m_pCmdQueue));
	if (FAILED(hr))
	{
		return false;
	}

	return true;
}

UINT64 GraphicsDevice::SignalQueue()
{
	auto hr = m_pCmdQueue->Signal(m_pFence.Get(), ++m_fenceVal);
	if (FAILED(hr))
	{
		return false;
	}
	return m_fenceVal;
}

void GraphicsDevice::WaitForFence(UINT64 value)
{
	if (m_pFence->GetCompletedValue() < value)
	{
		m_pFence->SetEventOnCompletion(value, m_fenceEvent);
		WaitForSingleObject(m_fenceEvent, INFINITE);
	}
}

bool GraphicsDevice::CreateSwapChain(HWND hWnd, int width, int height)
{
	DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
	swapchainDesc.Width = width;
	swapchainDesc.Height = height;
	swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapchainDesc.SampleDesc.Count = 1;
	swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapchainDesc.BufferCount = 2;
	swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapchainDesc.Flags = 0;
	// 縺ｾ縺・SwapChain1 縺ｨ縺励※蜿励￠繧・
	ComPtr<IDXGISwapChain1> swapChain1;
	HRESULT hr = m_pDxgiFactory->CreateSwapChainForHwnd(m_pCmdQueue.Get(), hWnd, &swapchainDesc,
		nullptr, nullptr, swapChain1.GetAddressOf()
	);

	if (FAILED(hr))
	{
		return false;
	}

	// SwapChain4 縺ｫ譏・ｼ
	hr = swapChain1.As(&m_pSwapChain);
	if (FAILED(hr))
	{
		return false;
	}
	return true;
}

bool GraphicsDevice::CreateSwapChainRTV()
{
	for (int i = 0; i < (int)m_pSwapchainBuffers.size(); ++i)
	{
		auto hr = m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&m_pSwapchainBuffers[i]));

		if (FAILED(hr))
		{
			return false;
		}
		m_pRTVHeap->CreateRTV(m_pSwapchainBuffers[i].Get());
	}

	return true;
}

bool GraphicsDevice::CreateFence()
{
	auto result = m_pDevice->CreateFence(m_fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_pFence));

	if (FAILED(result))
	{
		HRESULT hr = m_pDevice->CreateFence(
			m_fenceVal,
			D3D12_FENCE_FLAG_NONE,
			IID_PPV_ARGS(m_pFence.GetAddressOf())
		);

		if (FAILED(hr))
		{
			HRESULT removed = m_pDevice->GetDeviceRemovedReason();

			wchar_t buf[256];
			swprintf_s(buf,
				L"[CreateFence] hr=0x%08X removed=0x%08X fenceVal=%llu\n",
				(unsigned)hr, (unsigned)removed, (unsigned long long)m_fenceVal);
			OutputDebugStringW(buf);
			return false;
		}
	}

	return true;
}

void GraphicsDevice::SetResourceBarrier(ID3D12Resource* pResource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
{
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Transition.pResource = pResource;
	barrier.Transition.StateAfter = after;
	barrier.Transition.StateBefore = before;
	m_pCmdList->ResourceBarrier(1, &barrier);
}

void GraphicsDevice::EnableDebugLayer()
{
	ID3D12Debug* debugLayer = nullptr;

	D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer));
	debugLayer->EnableDebugLayer();
	debugLayer->Release();

}
void GraphicsDevice::Shutdown()
{
	if (!m_pDevice || !m_pCmdQueue || !m_pFence) return;

	// ImGui隗｣謾ｾ・・PU蠕・ｩ溷ｾ後↓陦後≧・・
	ShutdownImGui();

	// GPU螳御ｺ・ｾ・■
	WaitForCommandQueue();

	if (m_fenceEvent)
	{
		CloseHandle(m_fenceEvent);
		m_fenceEvent = nullptr;
	}
	// 縺薙％縺九ｉ蜈医〒隗｣謾ｾ・・omPtr/unique_ptr繧堤｢ｺ螳溘↓關ｽ縺ｨ縺呻ｼ・
	m_pSwapchainBuffers.fill(nullptr);
	m_pRTVHeap.reset();
	m_pSwapChain.Reset();

	m_pCmdList.Reset();
	for (int i = 0; i < kFrameCount; ++i)
	{
		m_frames[i].allocator.Reset();
		m_frames[i].fenceValue = 0;
	}
	m_pCmdQueue.Reset();

	m_pFence.Reset();
	m_pDevice.Reset();
	m_pDxgiFactory.Reset();
}
#include "GraphicsDevice.h"

bool GraphicsDevice::CreateDefaultTextures()
{
	ComPtr<ID3D12Resource> uploadBufferWhite;
	ComPtr<ID3D12Resource> uploadBufferBlack;
	ComPtr<ID3D12Resource> uploadBufferNormal;

	m_pCmdList->Reset(m_frames[0].allocator.Get(), nullptr);
	// White Tex
	m_spWhiteTex = std::make_unique<Texture>();
	{
		D3D12_HEAP_PROPERTIES heapProp = {};
		heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
		D3D12_RESOURCE_DESC recDesc = {};
		recDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		recDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		recDesc.Width = 1;
		recDesc.Height = 1;
		recDesc.DepthOrArraySize = 1;
		recDesc.MipLevels = 1;
		recDesc.SampleDesc.Count = 1;
		
		m_pDevice->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &recDesc,
			D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_spWhiteTex->m_pBuffer));
		
		heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
		recDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		recDesc.Format = DXGI_FORMAT_UNKNOWN;
		recDesc.Width = 256; 
		recDesc.Height = 1;
		recDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		
		m_pDevice->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &recDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadBufferWhite));
		
		uint32_t whitePixel = 0xFFFFFFFF;
		uint32_t* pMapped = nullptr;
		uploadBufferWhite->Map(0, nullptr, (void**)&pMapped);
		*pMapped = whitePixel;
		uploadBufferWhite->Unmap(0, nullptr);
		
		D3D12_TEXTURE_COPY_LOCATION dst = {};
		dst.pResource = m_spWhiteTex->m_pBuffer.Get();
		dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dst.SubresourceIndex = 0;
		
		D3D12_TEXTURE_COPY_LOCATION src = {};
		src.pResource = uploadBufferWhite.Get();
		src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		src.PlacedFootprint.Offset = 0;
		src.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		src.PlacedFootprint.Footprint.Width = 1;
		src.PlacedFootprint.Footprint.Height = 1;
		src.PlacedFootprint.Footprint.Depth = 1;
		src.PlacedFootprint.Footprint.RowPitch = 256;
		
		m_pCmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
		
		SetResourceBarrier(m_spWhiteTex->m_pBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
		
		m_spWhiteTex->m_srvNumber = m_upCBVSRVUAVHeap->CreateSRV(m_spWhiteTex->m_pBuffer.Get());
		m_spWhiteTex->m_pGraphicsDevice = this;
	}
	
	// Black Tex
	m_spBlackTex = std::make_unique<Texture>();
	{
		D3D12_HEAP_PROPERTIES heapProp = {};
		heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
		D3D12_RESOURCE_DESC recDesc = {};
		recDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		recDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		recDesc.Width = 1;
		recDesc.Height = 1;
		recDesc.DepthOrArraySize = 1;
		recDesc.MipLevels = 1;
		recDesc.SampleDesc.Count = 1;
		
		m_pDevice->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &recDesc,
			D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_spBlackTex->m_pBuffer));
		
		heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
		recDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		recDesc.Format = DXGI_FORMAT_UNKNOWN;
		recDesc.Width = 256;
		recDesc.Height = 1;
		recDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		
		m_pDevice->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &recDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadBufferBlack));
		
		uint32_t blackPixel = 0xFF000000;
		uint32_t* pMapped = nullptr;
		uploadBufferBlack->Map(0, nullptr, (void**)&pMapped);
		*pMapped = blackPixel;
		uploadBufferBlack->Unmap(0, nullptr);
		
		D3D12_TEXTURE_COPY_LOCATION dst = {};
		dst.pResource = m_spBlackTex->m_pBuffer.Get();
		dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dst.SubresourceIndex = 0;
		
		D3D12_TEXTURE_COPY_LOCATION src = {};
		src.pResource = uploadBufferBlack.Get();
		src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		src.PlacedFootprint.Offset = 0;
		src.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		src.PlacedFootprint.Footprint.Width = 1;
		src.PlacedFootprint.Footprint.Height = 1;
		src.PlacedFootprint.Footprint.Depth = 1;
		src.PlacedFootprint.Footprint.RowPitch = 256;
		
		m_pCmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
		
		SetResourceBarrier(m_spBlackTex->m_pBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
		
		m_spBlackTex->m_srvNumber = m_upCBVSRVUAVHeap->CreateSRV(m_spBlackTex->m_pBuffer.Get());
		m_spBlackTex->m_pGraphicsDevice = this;
	}

	// Normal Tex (RGB=127,127,255)
	m_spNormalTex = std::make_unique<Texture>();
	{
		D3D12_HEAP_PROPERTIES heapProp = {};
		heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
		D3D12_RESOURCE_DESC recDesc = {};
		recDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		recDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		recDesc.Width = 1;
		recDesc.Height = 1;
		recDesc.DepthOrArraySize = 1;
		recDesc.MipLevels = 1;
		recDesc.SampleDesc.Count = 1;
		
		m_pDevice->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &recDesc,
			D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_spNormalTex->m_pBuffer));
		
		heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
		recDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		recDesc.Format = DXGI_FORMAT_UNKNOWN;
		recDesc.Width = 256;
		recDesc.Height = 1;
		recDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		
		m_pDevice->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &recDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadBufferNormal));
		
		uint32_t normalPixel = 0xFFFF7F7F; // A=255, B=255, G=127, R=127
		uint32_t* pMapped = nullptr;
		uploadBufferNormal->Map(0, nullptr, (void**)&pMapped);
		*pMapped = normalPixel;
		uploadBufferNormal->Unmap(0, nullptr);
		
		D3D12_TEXTURE_COPY_LOCATION dst = {};
		dst.pResource = m_spNormalTex->m_pBuffer.Get();
		dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dst.SubresourceIndex = 0;
		
		D3D12_TEXTURE_COPY_LOCATION src = {};
		src.pResource = uploadBufferNormal.Get();
		src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		src.PlacedFootprint.Offset = 0;
		src.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		src.PlacedFootprint.Footprint.Width = 1;
		src.PlacedFootprint.Footprint.Height = 1;
		src.PlacedFootprint.Footprint.Depth = 1;
		src.PlacedFootprint.Footprint.RowPitch = 256;
		
		m_pCmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
		
		SetResourceBarrier(m_spNormalTex->m_pBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
		
		m_spNormalTex->m_srvNumber = m_upCBVSRVUAVHeap->CreateSRV(m_spNormalTex->m_pBuffer.Get());
		m_spNormalTex->m_pGraphicsDevice = this;

		m_pCmdList->Close();
		ID3D12CommandList* cmdlists[] = { m_pCmdList.Get() };
		m_pCmdQueue->ExecuteCommandLists(1, cmdlists);
		WaitForCommandQueue();
		m_frames[0].allocator->Reset();
		m_pCmdList->Reset(m_frames[0].allocator.Get(), nullptr);
	}

	m_pCmdList->Close();
	return true;
}

// =============================================
// GraphicsDevice.cpp (ImGui邨ｱ蜷磯Κ蛻・
// =============================================

bool GraphicsDevice::InitImGui()
{
	// ImGui逕ｨ縺ｮSRV繝・せ繧ｯ繝ｪ繝励ち繝偵・繝励ｒ菴懈・・医ヵ繧ｩ繝ｳ繝医ユ繧ｯ繧ｹ繝√Ε逕ｨ・・
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.NumDescriptors = 100;
	heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	HRESULT hr = m_pDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_upImGuiSRVHeap));
	if (FAILED(hr))
	{
		return false;
	}

	// ImGui繧ｳ繝ｳ繝・く繧ｹ繝育函謌・
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;	// 繝峨ャ繧ｭ繝ｳ繧ｰ譛牙柑蛹・

	// 繧ｹ繧ｿ繧､繝ｫ險ｭ螳・
	ImGui::StyleColorsDark();

    // Load Japanese font
    io.Fonts->AddFontFromFileTTF("c:/Windows/Fonts/meiryo.ttc", 16.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());

	// Win32繝舌ャ繧ｯ繧ｨ繝ｳ繝峨・蛻晄悄蛹悶・Win32Window縺梧戟縺､hWnd縺悟ｿ・ｦ√↑縺ｮ縺ｧ
	// 縺薙％縺ｧ縺ｯ ImGui_ImplDX12縺ｮ縺ｿ蛻晄悄蛹悶８in32縺ｮ蛻晄悄蛹悶・main/Application縺ｫ莉ｻ縺帙ｋ縲・
	// 窶ｻ ImGui_ImplWin32_Init(hWnd) 縺ｯ Application::Init()遲峨〒蜻ｼ縺ｶ縺薙→
	ImGui_ImplDX12_InitInfo initInfo = {};
	initInfo.Device            = m_pDevice.Get();
	initInfo.CommandQueue      = m_pCmdQueue.Get();
	initInfo.NumFramesInFlight = kFrameCount;
	initInfo.RTVFormat         = DXGI_FORMAT_R8G8B8A8_UNORM;
	initInfo.SrvDescriptorHeap = m_upImGuiSRVHeap.Get();
	initInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu)
	{
		// 莉翫・繝輔か繝ｳ繝育畑縺ｫ1繧ｹ繝ｭ繝・ヨ縺縺大崋螳壹〒蜑ｲ繧雁ｽ薙※
		auto& dev = GraphicsDevice::Instance();
		int idx = dev.AllocateImGuiSRVIndex();
		
		auto cpuHandle = dev.m_upImGuiSRVHeap->GetCPUDescriptorHandleForHeapStart();
		cpuHandle.ptr += dev.m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * idx;
		
		auto gpuHandle = dev.m_upImGuiSRVHeap->GetGPUDescriptorHandleForHeapStart();
		gpuHandle.ptr += dev.m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * idx;
		
		*out_cpu = cpuHandle;
		*out_gpu = gpuHandle;
	};
	initInfo.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE) {};

	if (!ImGui_ImplDX12_Init(&initInfo))
	{
		return false;
	}

	return true;
}

void GraphicsDevice::RenderImGui()
{
	// ImGui謠冗判繧ｳ繝槭Φ繝峨ｒ繧ｳ繝槭Φ繝峨Μ繧ｹ繝医↓遨阪・
	ImGui::Render();

	// ImGui蟆ら畑繝偵・繝励ｒ繧ｻ繝・ヨ
	ID3D12DescriptorHeap* ppHeaps[] = { m_upImGuiSRVHeap.Get() };
	m_pCmdList->SetDescriptorHeaps(1, ppHeaps);

	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_pCmdList.Get());
}

void GraphicsDevice::ShutdownImGui()
{
	ImGui_ImplDX12_Shutdown();
	// Win32繝舌ャ繧ｯ繧ｨ繝ｳ繝芽ｧ｣謾ｾ・・estroyContext蜑阪↓蠢・★蜻ｼ縺ｶ・・
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	m_upImGuiSRVHeap.Reset();
}

void GraphicsDevice::SetRenderTarget(RenderTarget* pRT)
{
	auto rtvH = m_pRTVHeap->GetCPUHandle(pRT->GetRTVIndex());
	auto dsvH = m_upDSVHeap->GetCPUHandle(pRT->GetDSVIndex());

	m_pCmdList->OMSetRenderTargets(1, &rtvH, false, &dsvH);
}

void GraphicsDevice::SetBackBuffer()
{
	auto bbIdx = m_pSwapChain->GetCurrentBackBufferIndex();
	auto rtvH = m_pRTVHeap->GetCPUHandle(bbIdx);
	auto dsvH = m_upDSVHeap->GetCPUHandle(m_upDepthStencil->GetDSVNumber());

	m_pCmdList->OMSetRenderTargets(1, &rtvH, false, &dsvH);
}

void GraphicsDevice::ClearBackBuffer(float r, float g, float b, float a)
{
	auto bbIdx = m_pSwapChain->GetCurrentBackBufferIndex();
	auto rtvH = m_pRTVHeap->GetCPUHandle(bbIdx);
	float clearColor[] = { r, g, b, a };
	m_pCmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);
}

int GraphicsDevice::AllocateImGuiSRV(ID3D12Resource* pBuffer)
{
	int index = m_imGuiSrvCount++;
	D3D12_CPU_DESCRIPTOR_HANDLE handle = m_upImGuiSRVHeap->GetCPUDescriptorHandleForHeapStart();
	
	UINT incrementSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	handle.ptr += static_cast<SIZE_T>(index) * incrementSize;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = pBuffer->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	
	m_pDevice->CreateShaderResourceView(pBuffer, &srvDesc, handle);
	return index;
}

D3D12_GPU_DESCRIPTOR_HANDLE GraphicsDevice::GetImGuiSRVGPUHandle(int index)
{
	D3D12_GPU_DESCRIPTOR_HANDLE handle = m_upImGuiSRVHeap->GetGPUDescriptorHandleForHeapStart();
	UINT incrementSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	handle.ptr += static_cast<SIZE_T>(index) * incrementSize;
	return handle;
}
