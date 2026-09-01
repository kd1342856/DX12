#pragma once
#include <mutex>

class DescriptorHeapManager;
class FrameConstantBufferAllocator;
class DepthStencil;
class Texture;
class ResourceUploader;
class ResourceStateTracker;
class RenderTarget;
class ResourceLifetimeManager;

#include <GraphicsMemory.h>
#include <SpriteBatch.h>
#include "Frame/FrameManager.h"
#include "Queue/QueueManager.h"
#include "Context/ContextManager.h"

class GraphicsDevice {
public:
	// 終了処理が始まったかどうか。単純なstatic boolなので、GraphicsDeviceの
	// インスタンス自体が(静的破棄順序の都合等で)既に壊れていてもここだけは安全に読める。
	// GPUResourceのデストラクタ等、終了処理の後にどのタイミングで呼ばれるか
	// 確定できない箇所から、m_deviceに触る前に必ずこれを確認すること。
	static bool IsShuttingDown() { return s_isShuttingDown; }

	// 初期化
	bool Init(HWND hWnd, int w, int h);
	
	// 描画終了
	void EndFrame();

	// 描画開始
	void BeginFrame();

	
	// レンダーターゲット設定
	void SetRenderTarget(RenderTarget* pRT);
	void TransitionToSRV(RenderTarget* pRT);
	void SetBackBuffer();
	void ClearBackBuffer(float r, float g, float b, float a);

	// ImGui描画(EndFrame前に呼ぶ)
	void RenderImGui();

	// Getter
	ID3D12Device8* GetDevice()const						{ return m_pDevice.Get(); }
	
	ID3D12GraphicsCommandList6* GetCmdList() const { return m_upContextManager->GetGraphicsContext()->GetCmdList(); }
	ContextManager* GetContextManager() const { return m_upContextManager.get(); }
	ResourceUploader* GetResourceUploader()const { return m_upResourceUploader.get(); }
	ResourceStateTracker* GetResourceStateTracker() const { return m_upResourceStateTracker.get(); }
	ResourceLifetimeManager* GetResourceLifetimeManager() const { return m_upResourceLifetimeManager.get(); }
	QueueManager* GetQueueManager() const { return m_upQueueManager.get(); }
	FrameManager* GetFrameManager() const { return m_upFrameManager.get(); }
	DescriptorHeapManager* GetDescriptorHeapManager()const { return m_upDescriptorHeapManager.get(); }
	FrameConstantBufferAllocator* GetFrameConstantBufferAllocator() { return m_upFrameManager->GetCurrentFrameResource().GetConstantBufferAllocator(); }

	const FrameConstantBufferAllocator* GetFrameConstantBufferAllocator() const { return m_upFrameManager->GetCurrentFrameResource().GetConstantBufferAllocator(); }
	DepthStencil* GetDepthStencil()const { return m_upDepthStencil.get(); } 
	DepthStencil* GetShadowMap()const { return m_upShadowMap.get(); }
	


	// Safe deferred GPU resource deletion.
	// Call this instead of directly resetting a ComPtr that the GPU may still reference.
	// fenceValue: the current Graphics Queue fence value (use GetQueueManager()->GetGraphicsQueue()->GetFenceValue())
	void DeferDeleteResource(Microsoft::WRL::ComPtr<ID3D12Resource> resource, uint64_t fenceValue);
	
	Texture* GetWhiteTex()const { return m_spWhiteTex.get(); }
	Texture* GetBlackTex()const { return m_spBlackTex.get(); }
	Texture* GetNormalTex()const { return m_spNormalTex.get(); }


	// SpriteBatch
	DirectX::SpriteBatch* GetSpriteBatch() const { return m_spSpriteBatch.get(); }

	// 終了処理
	void Shutdown();
	void EnableDebugLayer();

	ID3D12DescriptorHeap* GetImGuiSRVHeap() const { return m_upImGuiSRVHeap.Get(); }

	// SpotShadowMapは未使用のため削除済み
	int m_imGuiSrvCount = 1;

	std::unique_ptr<DirectX::SpriteBatch> m_spSpriteBatch;

	int AllocateImGuiSRV(ID3D12Resource* pBuffer);
	int AllocateImGuiSRVIndex() { return m_imGuiSrvCount++; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetImGuiSRVGPUHandle(int index);

	int CreateSRV(ID3D12Resource* pBuffer);
	int CreateRTV(ID3D12Resource* pBuffer);
	int CreateDSV(ID3D12Resource* pBuffer, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN);

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
	Microsoft::WRL::ComPtr<IDXGIAdapter3>					m_pAdapter3 = nullptr;

	// コマンド
			
	// スワップチェーン
	Microsoft::WRL::ComPtr<IDXGISwapChain4>					m_pSwapChain = nullptr;

	// Number of swap chain back buffers. 3 lets the CPU run ahead of the GPU.
	static constexpr UINT kSwapChainBufferCount = 3;

	// True when the adapter/OS supports tearing. Required to actually
	// bypass DWM vsync in windowed mode.
	bool m_allowTearing = false;

	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, kSwapChainBufferCount>	m_pSwapchainBuffers;


	
	std::unique_ptr<QueueManager> m_upQueueManager = nullptr;
	std::unique_ptr<FrameManager> m_upFrameManager = nullptr;
	std::unique_ptr<ContextManager> m_upContextManager = nullptr;
	std::unique_ptr<DescriptorHeapManager> m_upDescriptorHeapManager = nullptr;

	std::unique_ptr<DepthStencil>			m_upDepthStencil = nullptr;
	std::unique_ptr<DepthStencil>			m_upShadowMap = nullptr;
	std::unique_ptr<ResourceStateTracker> m_upResourceStateTracker = nullptr;
	std::unique_ptr<ResourceUploader> m_upResourceUploader = nullptr;
	std::unique_ptr<ResourceLifetimeManager> m_upResourceLifetimeManager = nullptr;
		std::unique_ptr<Texture> m_spWhiteTex = nullptr;
	std::unique_ptr<Texture> m_spBlackTex = nullptr;
	std::unique_ptr<Texture> m_spNormalTex = nullptr;

	// ImGui用SRV用ヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>			m_upImGuiSRVHeap = nullptr;
	std::unique_ptr<DirectX::GraphicsMemory> m_graphicsMemory = nullptr;
	float GetVRAMUsageMB();
private:
	bool CreateFactory();
	bool CreateDevice();
				bool CreateSwapChain(HWND hWnd, int width, int height);
	bool CreateSwapChainRTV();
		bool CreateDefaultTextures();

	// SpotShadowMap: 未使用のため削除済み

	// ImGui初期化
	bool InitImGui();
	void ShutdownImGui();

	GraphicsDevice();
	~GraphicsDevice();
public:
	static GraphicsDevice& Instance();

private:
	static inline bool s_isShuttingDown = false;
public:

};







