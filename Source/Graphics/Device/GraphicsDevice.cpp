#include "GraphicsDevice.h"

bool GraphicsDevice::Init(HWND  hWnd, int w, int h)
{
	if (!CreateFactory())
	{
		assert(0 && "ファクトリー作成失敗");
		return false;
	}
#ifdef _DEBUG
	EnableDebugLayer();
#endif
	if (!CreateDevice())
	{
		assert(0 && "D3D12デバイス作成失敗");
		return false;
	}
	if (!CreateCommandList())
	{
		assert(0 && "コマンドリストの作成失敗");
		return false;
	}
	if (!CreateSwapChain(hWnd, w, h))
	{
		assert(0 && "スワップチェインの作成失敗");
		return false;
	}
	m_pRTVHeap = std::make_unique<RTVHeap>();
	if (!m_pRTVHeap->Create(this, HeapType::RTV, 100))
	{
		assert(0 && "RTVヒープの作成失敗");
		return false;
	}
	m_upCBVSRVUAVHeap	= std::make_unique<CBVSRVUAVHeap>();
	if (!m_upCBVSRVUAVHeap->Create(this, HeapType::CBVSRVUAV, Math::Vector3(100, 100, 100)))
	{
		assert(0 && "CBVSRVUAVヒープの作成失敗");
		return false;
	}

	m_upCBufferAllocator = std::make_unique<CBufferAllocator>();
	m_upCBufferAllocator->Create(this, m_upCBVSRVUAVHeap.get());

	m_upDSVHeap = std::make_unique<DSVHeap>();
	if (!m_upDSVHeap->Create(this, HeapType::DSV, 100))
	{
		assert(0 && "DSVヒープの作成失敗");
		return false;
	}
	m_upDepthStencil = std::make_unique<DepthStencil>();
	if(!m_upDepthStencil->Create(this, Math::Vector2(w, h)))
	{
		assert(0 && "DepthStencilの作成失敗");
		return false;
	}
	if (!CreateSwapChainRTV())
	{
		assert(0 && "スワップチェインRTVの作成失敗");
		return false;
	}
	if (!CreateFence())
	{
		assert(0 && "フェンスの作成失敗");
		return false;
	}
	m_fenceEvent = CreateEvent(nullptr, false, false, nullptr);
	if (!m_fenceEvent)
	{
		assert(0 && "イベントエラー、アプリケーションを終了します");
		return false;
	}
	return true;
}

void GraphicsDevice::EndFrame()
{
	auto bbIdx = m_pSwapChain->GetCurrentBackBufferIndex();
	SetResourceBarrier(m_pSwapchainBuffers[bbIdx].Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

	m_pCmdList->Close();
	ID3D12CommandList* cmdlists[] = { m_pCmdList.Get() };
	m_pCmdQueue->ExecuteCommandLists(1, cmdlists);

	//	フレームの転出番号を記録
	const int frameIdx = m_frameIndex % kFrameCount;
	m_frames[frameIdx].fenceValue = SignalQueue();

	//	スワップチェインにプレゼント（送る）
	m_pSwapChain->Present(1, 0);
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

	float clearColor[] = { 0.0f,0.0f,1.0f,1.0f };	//	青
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

	//	使用中PCにあるGOUドライバーを検索して、あれば格納
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

	//	Direct3Dデバイスの初期化
	D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
	for (auto lv : levels)
	{
		if (D3D12CreateDevice(pSelectAdapter.Get(), lv, IID_PPV_ARGS(&m_pDevice)) == S_OK)
		{
			featureLevel = lv;
			break;	//生成可能なバージョンが見つかったらループ打ち切り
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

	//	キュー生成
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
	// まず SwapChain1 として受ける
	ComPtr<IDXGISwapChain1> swapChain1;
	HRESULT hr = m_pDxgiFactory->CreateSwapChainForHwnd(m_pCmdQueue.Get(), hWnd, &swapchainDesc,
		nullptr, nullptr, swapChain1.GetAddressOf()
	);

	if (FAILED(hr))
	{
		return false;
	}

	// SwapChain4 に昇格
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

	// GPU完了待ち
	WaitForCommandQueue();

	if (m_fenceEvent)
	{
		CloseHandle(m_fenceEvent);
		m_fenceEvent = nullptr;
	}
	// ここから先で解放（ComPtr/unique_ptrを確実に落とす）
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
