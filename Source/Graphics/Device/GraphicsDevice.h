#pragma once
#include <mutex>

class DescriptorHeapManager;

class FrameConstantBufferAllocator;

class DepthStencil;
class Texture;
class ResourceUploader;
class ResourceStateTracker;
class RenderTarget;

#include <GraphicsMemory.h>
#include <SpriteBatch.h>
#include "Frame/FrameManager.h"
#include "Queue/QueueManager.h"
#include "Context/ContextManager.h"

class GraphicsDevice {
public:
	// 初期匁E
	bool Init(HWND hWnd, int w, int h);
	
	// 描画終亁E
	void EndFrame();

	// 描画開姁E
	void BeginFrame();

	
	// レンダーターゲチE��設宁E
	void SetRenderTarget(RenderTarget* pRT);
	void SetBackBuffer();
	void ClearBackBuffer(float r, float g, float b, float a);

	// ImGui描画(EndFrame前に呼ぶ)
	void RenderImGui();

	// Getter
	ID3D12Device8* GetDevice()const						{ return m_pDevice.Get(); }
	
	ID3D12GraphicsCommandList6* GetCmdList() const { return m_upContextManager->GetGraphicsContext()->GetCmdList(); }
	ContextManager* GetContextManager() const { return m_upContextManager.get(); }
	ResourceUploader* GetResourceUploader()const { return m_upResourceUploader.get(); }
		QueueManager* GetQueueManager() const { return m_upQueueManager.get(); }
	FrameManager* GetFrameManager() const { return m_upFrameManager.get(); }
	DescriptorHeapManager* GetDescriptorHeapManager()const { return m_upDescriptorHeapManager.get(); }
	FrameConstantBufferAllocator* GetFrameConstantBufferAllocator() { return m_upFrameManager->GetCurrentFrameResource().GetConstantBufferAllocator(); }

	const FrameConstantBufferAllocator* GetFrameConstantBufferAllocator() const { return m_upFrameManager->GetCurrentFrameResource().GetConstantBufferAllocator(); }
	DepthStencil* GetDepthStencil()const { return m_upDepthStencil.get(); } 
	DepthStencil* GetShadowMap()const { return m_upShadowMap.get(); }
	
	
	Texture* GetWhiteTex()const { return m_spWhiteTex.get(); }
	Texture* GetBlackTex()const { return m_spBlackTex.get(); }
	Texture* GetNormalTex()const { return m_spNormalTex.get(); }

	// SpriteBatch
	DirectX::SpriteBatch* GetSpriteBatch() const { return m_spSpriteBatch.get(); }

	// 終亁E�E琁E
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

	// チE��イス
	Microsoft::WRL::ComPtr<ID3D12Device8>					m_pDevice = nullptr;
	Microsoft::WRL::ComPtr<IDXGIFactory6>					m_pDxgiFactory = nullptr;
	Microsoft::WRL::ComPtr<IDXGIAdapter3>					m_pAdapter3 = nullptr;

	// コマンチE
			
	// スワチE�Eチェーン
	Microsoft::WRL::ComPtr<IDXGISwapChain4>					m_pSwapChain = nullptr;

	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, 2>	m_pSwapchainBuffers;


	
	std::unique_ptr<QueueManager> m_upQueueManager = nullptr;
	std::unique_ptr<FrameManager> m_upFrameManager = nullptr;
	std::unique_ptr<ContextManager> m_upContextManager = nullptr;
	std::unique_ptr<DescriptorHeapManager> m_upDescriptorHeapManager = nullptr;

	std::unique_ptr<DepthStencil>			m_upDepthStencil = nullptr;
	std::unique_ptr<DepthStencil>			m_upShadowMap = nullptr;
	std::unique_ptr<ResourceUploader> m_upResourceUploader = nullptr;
		std::unique_ptr<Texture> m_spWhiteTex = nullptr;
	std::unique_ptr<Texture> m_spBlackTex = nullptr;
	std::unique_ptr<Texture> m_spNormalTex = nullptr;

	// ImGui用SRV用ヒ�EチE
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

	// ImGui初期匁E
	bool InitImGui();
	void ShutdownImGui();

	GraphicsDevice();
	~GraphicsDevice();
public:
	static GraphicsDevice& Instance();

};







