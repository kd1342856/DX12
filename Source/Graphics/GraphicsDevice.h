#pragma once

class RTVHeap;
class CBVSRVUAVHeap;
class CBufferAllocator;
class DSVHeap;
class DepthStencil;

class GraphicsDevice
{
public:

	//	初期化
	bool Init(HWND  hWnd, int w, int h);
	
	//	描画
	void ScreenFlip();

	//	描画準備
	void Prepare();

	void WaitForCommandQueue();

	//	Getter
	ID3D12Device8* GetDevice()const					{ return m_pDevice.Get(); }
	ID3D12GraphicsCommandList6* GetCmdList()const	{ return m_pCmdList.Get(); }
	CBVSRVUAVHeap* GetCBVSRVUAVHeap()const			{ return m_upCBVSRVUAVHeap.get(); }
	CBufferAllocator* GetCBufferAllocator()const	{ return m_upCBufferAllocator.get(); }
	DSVHeap* GetDSVHeap()const { return m_upDSVHeap.get(); }
	// Release
	void Shutdown();
	void EnableDebugLayer();

private:

	bool CreateFactory();

	bool CreateDevice();

	bool CreateCommandList();

	bool CreateSwapChain(HWND  hWnd, int width, int height);

	bool CreateSwapChainRTV();

	bool CreateFence();

	void SetResourceBarrier(ID3D12Resource* pResource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);



	enum class GPUTier
	{
		NVIDIA,
		Amd,
		Intel,
		Arm,
		Qualcomm,
		Kind,
	};

	//	デバイス
	ComPtr<ID3D12Device8>					m_pDevice = nullptr;
	ComPtr<IDXGIFactory6>					m_pDxgiFactory = nullptr;

	// コマンドリスト
	ComPtr<ID3D12CommandAllocator>			m_pCmdAllocator = nullptr;
	ComPtr<ID3D12GraphicsCommandList6>		m_pCmdList = nullptr;
	ComPtr<ID3D12CommandQueue>				m_pCmdQueue = nullptr;

	//	スワップチェイン
	ComPtr<IDXGISwapChain4>					m_pSwapChain = nullptr;

	std::array<ComPtr<ID3D12Resource>, 2>	m_pSwapchainBuffers;
	std::unique_ptr<RTVHeap>				m_pRTVHeap = nullptr;

	ComPtr<ID3D12Fence>						m_pFence = nullptr;
	UINT64									m_fenceVal = 0;

	std::unique_ptr<CBVSRVUAVHeap>			m_upCBVSRVUAVHeap = nullptr;
	std::unique_ptr<CBufferAllocator>		m_upCBufferAllocator = nullptr;
	std::unique_ptr<DSVHeap>				m_upDSVHeap = nullptr;
	std::unique_ptr<DepthStencil>			m_upDepthStencil = nullptr;


	GraphicsDevice() {}
	~GraphicsDevice() {}
public:
	static GraphicsDevice& Instance()
	{
		static GraphicsDevice instance;
		return instance;
	}

};