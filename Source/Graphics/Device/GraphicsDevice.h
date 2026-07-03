#pragma once

class RTVHeap;
class CBVSRVUAVHeap;
class CBufferAllocator;
class DSVHeap;
class DepthStencil;
class Texture;
class RenderTarget;

struct FrameContext
{
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
	UINT64 fenceValue = 0;
};

#include <GraphicsMemory.h>
#include <SpriteBatch.h>

class GraphicsDevice {
public:
	// 初期化
	bool Init(HWND hWnd, int w, int h);
	
	// 描画終了
	void EndFrame();

	// 描画開始
	void BeginFrame();

	void WaitForCommandQueue();

	// レンダーターゲット設定
	void SetRenderTarget(RenderTarget* pRT);
	void SetBackBuffer();
	void ClearBackBuffer(float r, float g, float b, float a);

	// ImGui描画(EndFrame前に呼ぶ)
	void RenderImGui();

	// Getter
	ID3D12Device8* GetDevice()const						{ return m_pDevice.Get(); }
	ID3D12GraphicsCommandList6* GetCmdList()const		{ return m_pCmdList.Get(); }
	CBVSRVUAVHeap* GetCBVSRVUAVHeap()const { return m_upCBVSRVUAVHeap.get(); }
	CBufferAllocator* GetCBufferAllocator()const { return m_upCBufferAllocator.get(); }
	DepthStencil* GetDepthStencil()const { return m_upDepthStencil.get(); }
	DepthStencil* GetShadowMap()const { return m_upShadowMap.get(); }
	RTVHeap* GetRTVHeap()const { return m_pRTVHeap.get(); }
	DSVHeap* GetDSVHeap()const { return m_upDSVHeap.get(); }
	Texture* GetWhiteTex()const { return m_spWhiteTex.get(); }
	Texture* GetBlackTex()const { return m_spBlackTex.get(); }
	Texture* GetNormalTex()const { return m_spNormalTex.get(); }

	// SpriteBatch
	DirectX::SpriteBatch* GetSpriteBatch() const { return m_spSpriteBatch.get(); }

	// 終了処理
	void Shutdown();
	void EnableDebugLayer();

	ID3D12DescriptorHeap* GetImGuiSRVHeap() const { return m_upImGuiSRVHeap.Get(); }

	DepthStencil* GetSpotShadowMap()const { return m_upSpotShadowMap.get(); }

public:
	int m_imGuiSrvCount = 1;

	std::unique_ptr<DirectX::SpriteBatch> m_spSpriteBatch;
private:
	bool CreateFactory();
	bool CreateDevice();
	bool CreateCommandList();
	UINT64 SignalQueue();
	void WaitForFence(UINT64 value);
	bool CreateSwapChain(HWND hWnd, int width, int height);
	bool CreateSwapChainRTV();
	bool CreateFence();
	bool CreateDefaultTextures();

	std::unique_ptr<DepthStencil>	m_upSpotShadowMap = nullptr;

	// ImGui初期化
	bool InitImGui();
	void ShutdownImGui();

public:
	int AllocateImGuiSRV(ID3D12Resource* pBuffer);
	int AllocateImGuiSRVIndex() { return m_imGuiSrvCount++; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetImGuiSRVGPUHandle(int index);

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

	// デバイス
	Microsoft::WRL::ComPtr<ID3D12Device8>					m_pDevice = nullptr;
	Microsoft::WRL::ComPtr<IDXGIFactory6>					m_pDxgiFactory = nullptr;

	// コマンド
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6>		m_pCmdList = nullptr;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue>				m_pCmdQueue = nullptr;
	static constexpr int					kFrameCount = 2;
	FrameContext							m_frames[kFrameCount];
	UINT									m_frameIndex = 0;

	// スワップチェーン
	Microsoft::WRL::ComPtr<IDXGISwapChain4>					m_pSwapChain = nullptr;

	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, 2>	m_pSwapchainBuffers;
	std::unique_ptr<RTVHeap>				m_pRTVHeap = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Fence>						m_pFence = nullptr;
	HANDLE									m_fenceEvent = nullptr;
	UINT64									m_fenceVal = 0;

	std::unique_ptr<CBVSRVUAVHeap>			m_upCBVSRVUAVHeap = nullptr;
	std::unique_ptr<CBufferAllocator>		m_upCBufferAllocator = nullptr;
	std::unique_ptr<DSVHeap>				m_upDSVHeap = nullptr;
	std::unique_ptr<DepthStencil>			m_upDepthStencil = nullptr;
	std::unique_ptr<DepthStencil>			m_upShadowMap = nullptr;
	std::unique_ptr<Texture> m_spWhiteTex = nullptr;
	std::unique_ptr<Texture> m_spBlackTex = nullptr;
	std::unique_ptr<Texture> m_spNormalTex = nullptr;

	// ImGui用SRV用ヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>			m_upImGuiSRVHeap = nullptr;
	std::unique_ptr<DirectX::GraphicsMemory> m_graphicsMemory = nullptr;

	GraphicsDevice() {}
	~GraphicsDevice() {}
public:
	static GraphicsDevice& Instance();
};


