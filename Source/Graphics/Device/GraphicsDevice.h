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
	// �I���������n�܂������ǂ����B�P����static bool�Ȃ̂ŁAGraphicsDevice��
	// �C���X�^���X���̂�(�ÓI�j�������̓s������)���ɉ��Ă��Ă����������͈��S�ɓǂ߂�B
	// GPUResource�̃f�X�g���N�^���A�I�������̌�ɂǂ̃^�C�~���O�ŌĂ΂�邩
	// �m��ł��Ȃ��ӏ�����Am_device�ɐG��O�ɕK��������m�F���邱�ƁB
	static bool IsShuttingDown() { return s_isShuttingDown; }

	// ������
	bool Init(HWND hWnd, int w, int h);
	
	// �`��I��
	void EndFrame();

	// �`��J�n
	void BeginFrame();

	
	// �����_�[�^�[�Q�b�g�ݒ�
	void SetRenderTarget(RenderTarget* pRT);
	void TransitionToSRV(RenderTarget* pRT);
	void SetBackBuffer();
	void ClearBackBuffer(float r, float g, float b, float a);

	// ImGui�`��(EndFrame�O�ɌĂ�)
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

	// �I������
	void Shutdown();
	void EnableDebugLayer();

	// Persisted toggle for the D3D12 debug layer (Debug builds only). It can only be
	// enabled/disabled before the device is created, so this doesn't take effect until
	// the next launch - see the checkbox in RendererPanel and the comment in Init().
	static bool IsDebugLayerRequested();
	static void SetDebugLayerRequested(bool enabled);
	// Whether the debug layer is actually active *this* run (decided once, in Init()) -
	// as opposed to IsDebugLayerRequested(), which reflects the checkbox and may already
	// differ if the user just toggled it and hasn't restarted yet.
	static bool IsDebugLayerActive() { return s_debugLayerActive; }

	ID3D12DescriptorHeap* GetImGuiSRVHeap() const { return m_upImGuiSRVHeap.Get(); }

	// SpotShadowMap�͖��g�p�̂��ߍ폜�ς�
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

	// �f�o�C�X
	Microsoft::WRL::ComPtr<ID3D12Device8>					m_pDevice = nullptr;
	Microsoft::WRL::ComPtr<IDXGIFactory6>					m_pDxgiFactory = nullptr;
	Microsoft::WRL::ComPtr<IDXGIAdapter3>					m_pAdapter3 = nullptr;

	// �R�}���h
			
	// �X���b�v�`�F�[��
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

	// ImGui�pSRV�p�q�[�v
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>			m_upImGuiSRVHeap = nullptr;
	std::unique_ptr<DirectX::GraphicsMemory> m_graphicsMemory = nullptr;
	float GetVRAMUsageMB();
private:
	bool CreateFactory();
	bool CreateDevice();
				bool CreateSwapChain(HWND hWnd, int width, int height);
	bool CreateSwapChainRTV();
		bool CreateDefaultTextures();

	// SpotShadowMap: ���g�p�̂��ߍ폜�ς�

	// ImGui������
	bool InitImGui();
	void ShutdownImGui();

	GraphicsDevice();
	~GraphicsDevice();
public:
	static GraphicsDevice& Instance();

private:
	static inline bool s_isShuttingDown = false;
	static inline bool s_debugLayerActive = false;
public:

};







