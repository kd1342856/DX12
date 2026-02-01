#pragma once

class RTVHeap;

class GraphicsDevice
{
public:

	//	初期化
	bool Init(HWND  hWnd, int w, int h);
	
	//	描画
	void ScreenFlip();

	void WaitForCommandQueue();


	//	Getter
	ComPtr<ID3D12Device8> GetDevice()				{ return m_pDevice; }
	ComPtr<ID3D12GraphicsCommandList6> GetCmdList() { return m_pCmdList; }

	// Release
	void Shutdown();
private:

	bool CreateFactory();

	bool CreateDevice();

	bool CreateCommandList();

	bool CreateSwapChain(HWND  hWnd, int width, int height);

	bool CreateSwapChainRTV();

	bool CreateFence();

	void SetResourceBarrier(ID3D12Resource* pResource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);

	void EnableDebugLayer();


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

	GraphicsDevice() {}
	~GraphicsDevice() {}
public:
	static GraphicsDevice& Instance()
	{
		static GraphicsDevice instance;
		return instance;
	}

};