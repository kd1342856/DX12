#include "../../Pch.h"
#include "ResourceUploader.h"
#include "../GPUResource/RenderTarget/RenderTarget.h"
#include "../../Framework/DirectX/Utility/Profiler.h"
#include "GraphicsDevice.h"
#include "../Shader/ShaderCompiler/ShaderCompiler.h"
#include "../Shader/ShaderManager/ShaderManager.h"
GraphicsDevice& GraphicsDevice::Instance()
{
	static GraphicsDevice instance;
	return instance;
}
bool GraphicsDevice::Init(HWND  hWnd, int w, int h)
{
	if (!CreateFactory()) return false;
#ifdef _DEBUG
	EnableDebugLayer();
#endif
	if (!CreateDevice()) return false;

	m_upQueueManager = std::make_unique<QueueManager>();
	if (!m_upQueueManager->Init(m_pDevice.Get())) return false;

	m_upContextManager = std::make_unique<ContextManager>();
	if (!m_upContextManager->Init(m_pDevice.Get())) return false;

	m_upDescriptorHeapManager = std::make_unique<DescriptorHeapManager>();
	if (!m_upDescriptorHeapManager->Init(m_pDevice.Get(), 100, 100, 30000, 100)) return false;

	m_upFrameManager = std::make_unique<FrameManager>();
	if (!m_upFrameManager->Init(this)) return false;

	
	m_upResourceUploader = std::make_unique<ResourceUploader>();
	m_upResourceUploader->Init(this);

	if (!CreateSwapChain(hWnd, w, h)) return false;

	m_upDepthStencil = std::make_unique<DepthStencil>();
	if (!m_upDepthStencil->Create(this, Math::Vector2(static_cast<float>(w), static_cast<float>(h)))) return false;

	m_upShadowMap = std::make_unique<DepthStencil>();
	if(!m_upShadowMap->Create(this, Math::Vector2(4096.0f, 4096.0f), DepthStencilFormat::DepthHighQuality, true)) return false;

	if (!CreateSwapChainRTV()) return false;
	
	m_graphicsMemory = std::make_unique<DirectX::GraphicsMemory>(m_pDevice.Get());
	CreateDefaultTextures();
	if (!InitImGui())
	{
		return false;
	}
	return true;
}
void GraphicsDevice::EndFrame()
{
	auto cmdList = m_upContextManager->GetGraphicsContext()->GetCmdList();
	RenderImGui();
	auto bbIdx = m_pSwapChain->GetCurrentBackBufferIndex();
	m_upContextManager->GetGraphicsContext()->GetResourceStateTracker()->TransitionResource(m_pSwapchainBuffers[bbIdx].Get(), D3D12_RESOURCE_STATE_PRESENT);
	m_upContextManager->GetGraphicsContext()->GetResourceStateTracker()->FlushResourceBarriers(cmdList);
	m_upContextManager->GetGraphicsContext()->Close();
	auto queue = m_upQueueManager->GetGraphicsQueue();
	queue->Execute(m_upContextManager->GetGraphicsContext());
	uint64_t fenceVal = queue->Signal();
	m_upFrameManager->GetCurrentFrameResource().SetFenceValue(fenceVal);
	m_pSwapChain->Present(0, 0);
	m_graphicsMemory->Commit(queue->GetQueue());
	m_upFrameManager->MoveNextFrame();
}
void GraphicsDevice::BeginFrame()
{
	Profiler::Instance().ResetPerFrameCounters();
	FrameResource& fr = m_upFrameManager->AcquireFrame();
	m_upContextManager->GetGraphicsContext()->Begin(fr);
	auto cmdList = m_upContextManager->GetGraphicsContext()->GetCmdList();
	auto bbIdx = m_pSwapChain->GetCurrentBackBufferIndex();
	m_upContextManager->GetGraphicsContext()->GetResourceStateTracker()->TransitionResource(m_pSwapchainBuffers[bbIdx].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
	m_upContextManager->GetGraphicsContext()->GetResourceStateTracker()->FlushResourceBarriers(cmdList);
	auto rtvH = m_upDescriptorHeapManager->GetRTVAllocator()->GetCPUHandle(bbIdx);
	auto dsvH = m_upDescriptorHeapManager->GetDSVAllocator()->GetCPUHandle(m_upDepthStencil->GetDSVNumber());
	cmdList->OMSetRenderTargets(1, &rtvH, false, &dsvH);
	float clearColor[] = { 0.0f,0.0f,1.0f,1.0f };
	cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);
	m_upDepthStencil->ClearBuffer();
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
	//	Ëè¥ÅEøÈÄïÔΩ®Ëç≥ÅE≠PCÁ∏∫ÅE´Á∏∫„ÇÅEΩãGOUÁπùÂ≥®ŒõÁπßÅE§Áπù„ÅEEÁπßÂëàÔΩ§ÊáÅEΩ¥ÅE¢Á∏∫Âä±‚ÄªÁ∏≤‚àö‚â†Áπß„ÉªEË≠¨ÅEº?E
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
	//	Direct3DÁπù„ÅE??ÁπßÅE§ÁπßÅEπÁ∏∫ÅEÆËõªÊôÅEÇÑËõπ„Éª
	D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
	for (auto lv : levels)
	{
		if (D3D12CreateDevice(pSelectAdapter.Get(), lv, IID_PPV_ARGS(&m_pDevice)) == S_OK)
		{
			featureLevel = lv;
			break;	//ÈÄï„ÅEEËúøÅEØÈñ≠ÅEΩÁ∏∫ÅE™Áπù„ÅEEÁπßÅE∏ÁπùÔΩßÁπùÔΩ≥Á∏∫ÁëöÔΩ¶‰πù‚ñΩÁ∏∫‰πù‚ñ≤Á∏∫Ê∫òÔΩâÁπùÔΩ´ÁπùÔΩºÁπùÁé≤ÈÅ†Á∏∫ÅE°?E??
		}
	}
	pSelectAdapter.As(&m_pAdapter3);
	return true;
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
	// Á∏∫ÅEæÁ∏∫„ÉªSwapChain1 Á∏∫ÅE®Á∏∫Âä±‚ÄªËúøÂä±ÅE†Áπß„Éª
	ComPtr<IDXGISwapChain1> swapChain1;
	HRESULT hr = m_pDxgiFactory->CreateSwapChainForHwnd(m_upQueueManager->GetGraphicsQueue()->GetQueue(), hWnd, &swapchainDesc,
		nullptr, nullptr, swapChain1.GetAddressOf()
	);
	if (FAILED(hr))
	{
		return false;
	}
	// SwapChain4 Á∏∫ÅE´?E??
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
		m_upContextManager->GetGraphicsContext()->GetResourceStateTracker()->AddResourceState(m_pSwapchainBuffers[i].Get(), D3D12_RESOURCE_STATE_PRESENT);
		if (FAILED(hr))
		{
			return false;
		}
		CreateRTV(m_pSwapchainBuffers[i].Get());
	}
	return true;
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
	if (!m_pDevice) return;

	if (m_upQueueManager)
		m_upQueueManager->GetGraphicsQueue()->Flush();

	ShaderCompiler::Terminate();
	ShutdownImGui();

	m_pSwapchainBuffers.fill(nullptr);
	m_pSwapChain.Reset();

	if (m_upFrameManager) m_upFrameManager->Shutdown();
	if (m_upContextManager) m_upContextManager->Shutdown();
	if (m_upQueueManager) m_upQueueManager->Shutdown();
	if (m_upDescriptorHeapManager) m_upDescriptorHeapManager->Release();

	m_pDevice.Reset();
	m_pDxgiFactory.Reset();
}
bool GraphicsDevice::CreateDefaultTextures()
{
	ComPtr<ID3D12Resource> uploadBufferWhite;
	ComPtr<ID3D12Resource> uploadBufferBlack;
	ComPtr<ID3D12Resource> uploadBufferNormal;
	m_upContextManager->GetGraphicsContext()->Begin(m_upFrameManager->GetCurrentFrameResource());
	auto cmdList = m_upContextManager->GetGraphicsContext()->GetCmdList();
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
			D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_spWhiteTex->m_resource));
		
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
		dst.pResource = m_spWhiteTex->m_resource.Get();
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
		
		cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
		
		m_upContextManager->GetGraphicsContext()->GetResourceStateTracker()->TransitionResource(m_spWhiteTex->m_resource.Get(), D3D12_RESOURCE_STATE_GENERIC_READ);
		m_upContextManager->GetGraphicsContext()->GetResourceStateTracker()->FlushResourceBarriers(cmdList);
		
		m_spWhiteTex->m_srvNumber = CreateSRV(m_spWhiteTex->m_resource.Get());
		m_spWhiteTex->m_device = this;
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
			D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_spBlackTex->m_resource));
		
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
		dst.pResource = m_spBlackTex->m_resource.Get();
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
		
		cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
		
		m_upContextManager->GetGraphicsContext()->GetResourceStateTracker()->TransitionResource(m_spBlackTex->m_resource.Get(), D3D12_RESOURCE_STATE_GENERIC_READ);
		m_upContextManager->GetGraphicsContext()->GetResourceStateTracker()->FlushResourceBarriers(cmdList);
		
		m_spBlackTex->m_srvNumber = CreateSRV(m_spBlackTex->m_resource.Get());
		m_spBlackTex->m_device = this;
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
			D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_spNormalTex->m_resource));
		
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
		dst.pResource = m_spNormalTex->m_resource.Get();
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
		
		cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
		
		m_upContextManager->GetGraphicsContext()->GetResourceStateTracker()->TransitionResource(m_spNormalTex->m_resource.Get(), D3D12_RESOURCE_STATE_GENERIC_READ);
		m_upContextManager->GetGraphicsContext()->GetResourceStateTracker()->FlushResourceBarriers(cmdList);
		
		m_spNormalTex->m_srvNumber = CreateSRV(m_spNormalTex->m_resource.Get());
		m_spNormalTex->m_device = this;
	}

	m_upContextManager->GetGraphicsContext()->Close();
	m_upQueueManager->GetGraphicsQueue()->Execute(m_upContextManager->GetGraphicsContext());
	m_upQueueManager->GetGraphicsQueue()->Flush();

	return true;
}
// =============================================
// GraphicsDevice.cpp (ImGuiÈÇ®ÅE±Ëú∑Á£ØŒÅEE
// =============================================
bool GraphicsDevice::InitImGui()
{
	// ImGuiÈÄïÔΩ®Á∏∫ÅEÆSRVÁπù„ÅE??ÁπßÅEØÁπùÔΩ™ÁπùÂä±„Å°Áπù„ÅEEÁπùÂä±ÅEíËè¥„ÉªE?E?ÁπùËºî„ÅãÁπùÔΩ≥ÁπùÂåª„É¶ÁπßÅEØÁπßÅEπÁπù‚ÅEŒïÈÄïÔΩ®?E?E
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.NumDescriptors = 100;
	heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	HRESULT hr = m_pDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_upImGuiSRVHeap));
	if (FAILED(hr))
	{
		return false;
	}
	// ImGuiÁπßÅE≥ÁπùÔΩ≥Áπù„ÅE??ÁπßÅEπÁπùËÇ≤ÂáΩ?E
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;	// ÁπùÂ≥®„É£ÁπßÅE≠ÁπùÔΩ≥ÁπßÅE∞Ë≠õÁâôÊüëËõπ„Éª
	// ÁπßÅEπÁπßÅEøÁπßÅE§ÁπùÔΩ´Èö™ÅE≠?E
	ImGui::StyleColorsDark();
    // Load Japanese font
    io.Fonts->AddFontFromFileTTF("c:/Windows/Fonts/meiryo.ttc", 16.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
	// Win32ÁπùËÅE„É£ÁπßÅEØÁπßÅE®ÁπùÔΩ≥Áπù„ÅEEËõªÊôÅEÇÑËõπ„ÉªEWin32WindowÁ∏∫Ê¢ßÊàüÁ∏∫ÅE§hWndÁ∏∫„ÉªE??Á∏∫ÅE™Á∏∫ÅEÆÁ∏∫ÅEß
	// Á∏∫ËñôÔºÅE∏∫ÅEßÁ∏∫ÅEØ ImGui_ImplDX12Á∏∫ÅEÆÁ∏∫ÅEøËõªÊôÅEÇÑËõπÊÇ∂¬ÄÅEòin32Á∏∫ÅEÆËõªÊôÅEÇÑËõπ„ÉªEmain/ApplicationÁ∏∫ÅE´ËéâÔΩªÁ∏∫Â∏ôÔΩãÁ∏≤„Éª
	// Á™∂ÅEª ImGui_ImplWin32_Init(hWnd) Á∏∫ÅEØ Application::Init()ÈÅ≤Â≥®„ÄíËúªÅEºÁ∏∫ÅE∂Á∏∫Ëñô‚ÅE
	ImGui_ImplDX12_InitInfo initInfo = {};
	initInfo.Device            = m_pDevice.Get();
	initInfo.CommandQueue      = m_upQueueManager->GetGraphicsQueue()->GetQueue();
	initInfo.NumFramesInFlight = FrameManager::kFrameCount;
	initInfo.RTVFormat         = DXGI_FORMAT_R8G8B8A8_UNORM;
	initInfo.SrvDescriptorHeap = m_upImGuiSRVHeap.Get();
	initInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu)
	{
		// Ëéâ„ÅEEÁπùËºî„ÅãÁπùÔΩ≥ÁπùËÇ≤ÁïëÁ∏∫ÅE´1ÁπßÅEπÁπùÔΩ≠Áπù„ÅE????Á∏∫Â§ßÂ¥ãËû≥Â£π„ÄíËúëÅE≤ÁπßÈõÅEΩΩËñô‚Äª
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
	ImGui::Render();
	ID3D12DescriptorHeap* ppHeaps[] = { m_upImGuiSRVHeap.Get() };
	auto cmdList = m_upContextManager->GetGraphicsContext()->GetCmdList();
	cmdList->SetDescriptorHeaps(1, ppHeaps);
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList);
}
void GraphicsDevice::ShutdownImGui()
{
	ImGui_ImplDX12_Shutdown();
	// Win32ÁπùËÅE„É£ÁπßÅEØÁπßÅE®ÁπùÔΩ≥ÁπùËäΩÅEßÅE£Ë¨æÅEæ?E?EestroyContextËúëÈò™‚ÜÅEE??ËúªÅEºÁ∏∫ÅE∂?E?E
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	m_upImGuiSRVHeap.Reset();
}
void GraphicsDevice::SetRenderTarget(RenderTarget* pRT)
{
	auto rtvH = m_upDescriptorHeapManager->GetRTVAllocator()->GetCPUHandle(pRT->GetRTVIndex());
	auto dsvH = m_upDescriptorHeapManager->GetDSVAllocator()->GetCPUHandle(pRT->GetDSVIndex());
	m_upContextManager->GetGraphicsContext()->GetCmdList()->OMSetRenderTargets(1, &rtvH, false, &dsvH);
}
void GraphicsDevice::SetBackBuffer()
{
	auto bbIdx = m_pSwapChain->GetCurrentBackBufferIndex();
	auto rtvH = m_upDescriptorHeapManager->GetRTVAllocator()->GetCPUHandle(bbIdx);
	auto dsvH = m_upDescriptorHeapManager->GetDSVAllocator()->GetCPUHandle(m_upDepthStencil->GetDSVNumber());
	m_upContextManager->GetGraphicsContext()->GetCmdList()->OMSetRenderTargets(1, &rtvH, false, &dsvH);
}
void GraphicsDevice::ClearBackBuffer(float r, float g, float b, float a)
{
	auto bbIdx = m_pSwapChain->GetCurrentBackBufferIndex();
	auto rtvH = m_upDescriptorHeapManager->GetRTVAllocator()->GetCPUHandle(bbIdx);
	float clearColor[] = { r, g, b, a };
	m_upContextManager->GetGraphicsContext()->GetCmdList()->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);
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

float GraphicsDevice::GetVRAMUsageMB()
{
	if (m_pAdapter3)
	{
		DXGI_QUERY_VIDEO_MEMORY_INFO info = {};
		if (SUCCEEDED(m_pAdapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info)))
		{
			return static_cast<float>(info.CurrentUsage) / (1024.0f * 1024.0f);
		}
	}
	return 0.0f;
}

GraphicsDevice::~GraphicsDevice() {}
GraphicsDevice::GraphicsDevice() {}
int GraphicsDevice::CreateSRV(ID3D12Resource* pBuffer)
{
    int index = m_upDescriptorHeapManager->GetCBVSRVUAVAllocator()->Allocate();
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = pBuffer->GetDesc().Format;
    if (srvDesc.Format == DXGI_FORMAT_R32_TYPELESS) srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    if (srvDesc.Format == DXGI_FORMAT_R16_TYPELESS) srvDesc.Format = DXGI_FORMAT_R16_UNORM;
    if (srvDesc.Format == DXGI_FORMAT_R24G8_TYPELESS) srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = pBuffer->GetDesc().MipLevels;
    m_pDevice->CreateShaderResourceView(pBuffer, &srvDesc, m_upDescriptorHeapManager->GetCBVSRVUAVAllocator()->GetCPUHandle(index));
    return index;
}

int GraphicsDevice::CreateRTV(ID3D12Resource* pBuffer)
{
    int index = m_upDescriptorHeapManager->GetRTVAllocator()->Allocate();
    m_pDevice->CreateRenderTargetView(pBuffer, nullptr, m_upDescriptorHeapManager->GetRTVAllocator()->GetCPUHandle(index));
    return index;
}

int GraphicsDevice::CreateDSV(ID3D12Resource* pBuffer, DXGI_FORMAT format)
{
    int index = m_upDescriptorHeapManager->GetDSVAllocator()->Allocate();
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = (format == DXGI_FORMAT_UNKNOWN) ? pBuffer->GetDesc().Format : format;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
    m_pDevice->CreateDepthStencilView(pBuffer, &dsvDesc, m_upDescriptorHeapManager->GetDSVAllocator()->GetCPUHandle(index));
    return index;
}
















