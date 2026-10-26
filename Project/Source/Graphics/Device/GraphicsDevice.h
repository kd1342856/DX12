#pragma once
#include <mutex>
#include <array>

class DescriptorHeapManager;
class FrameConstantBufferAllocator;
class DepthStencil;
class ShadowMapArray;
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
	// インスタンス自体が(静的破棄順序の都合などで)既に壊れていても、これだけは安全に読める。
	// GPUResourceのデストラクタが、終了処理の後にどのタイミングで呼ばれるか
	// 確定できない箇所からは、m_deviceに触る前に必ずこれを確認すること。
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
	// RenderTarget専用の深度バッファ(pRT->GetDepthSRVIndex())の状態遷移。
	// DEPTH_WRITE(描画時) <-> PIXEL_SHADER_RESOURCE(DOF/SSAO/SSR等で読む時) を切り替える。
	void TransitionDepthToSRV(RenderTarget* pRT);
	void TransitionDepthToWrite(RenderTarget* pRT);
	void SetBackBuffer();
	void ClearBackBuffer(float r, float g, float b, float a);

	// ImGui描画(EndFrame前に呼ぶ)
	void RenderImGui();
	// ImGui_ImplWin32_NewFrame()の直後に呼ぶ。全画面でクライアント領域とバックバッファの
	// 大きさが異なる時、マウス座標(クライアント座標)のまま正しくバックバッファへ描けるようにする。
	void ApplyImGuiFramebufferScale();

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
	// 局所光源の影。狭いスポットは2D配列(Texture2DArray)、点光源と広いスポットはキューブ配列(TextureCubeArray)
	ShadowMapArray* GetSpotShadowMaps() const { return m_upSpotShadowMaps.get(); }
	ShadowMapArray* GetCubeShadowMaps() const { return m_upCubeShadowMaps.get(); }
	


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

	// D3D12デバッグレイヤーの永続化トグル(Debugビルドのみ)。デバイス作成前にしか
	// 有効/無効を切り替えられないため、次回起動まで反映されない
	// - RendererPanelのチェックボックスとInit()内のコメントを参照。
	static bool IsDebugLayerRequested();
	static void SetDebugLayerRequested(bool enabled);
	// このセッションで実際にデバッグレイヤーが有効かどうか(Init()で一度だけ決まる) -
	// IsDebugLayerRequested()はチェックボックスの状態を反映するもので、ユーザーが
	// 今しがた切り替えて未再起動の場合はこの値と食い違うことがある。
	static bool IsDebugLayerActive() { return s_debugLayerActive; }

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

	// fpsの上限の決め方
	enum class FrameRateMode
	{
		Unlimited, // 上限なし(ティアリングを許して即座に表示)
		VSync,     // モニターのリフレッシュレートに合わせる
		Limited,   // CPU側で待ち、GetFrameRateLimit()のfpsに合わせる
	};
	void SetFrameRateMode(FrameRateMode mode) { m_frameRateMode = mode; }
	FrameRateMode GetFrameRateMode() const { return m_frameRateMode; }
	void SetFrameRateLimit(int fps) { m_frameRateLimit = (std::max)(fps, 1); }
	int GetFrameRateLimit() const { return m_frameRateLimit; }

	// 表示待ちにしてよいフレームの数。1にするとCPUとGPUが交互にしか動けなくなるので2にしている。
	// これを超えて先行しないよう、BeginFrame()の先頭でスワップチェーンの待機オブジェクトを待つ
	static constexpr UINT kMaxFrameLatency = 2;

	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, kSwapChainBufferCount>	m_pSwapchainBuffers;
	std::array<int, kSwapChainBufferCount> m_swapchainRtvIndices = {};

	// ウィンドウの大きさが変わったとき(WM_SIZE)に呼ぶ。実際の作り直しは次のフレームの先頭で行う
	void RequestResize(UINT width, UINT height);
	// 大きさの変更が要求されていれば、GPUの処理を待ってスワップチェーンとメインの深度を作り直す。
	// 作り直したらtrue(呼び出し側で画面サイズのRTも作り直す)。フレームの先頭(コマンドを記録する前)で呼ぶ
	bool ApplyPendingResize();
	// バックバッファ(ウィンドウのクライアント領域)の大きさ
	UINT GetBackBufferWidth() const { return m_backBufferWidth; }
	UINT GetBackBufferHeight() const { return m_backBufferHeight; }

	//------------------------------------------
	// HDR出力(HDR10: BT.2020の色域とPQのカーブ、10bitのスワップチェーン)
	//   HDR出力のときは、ポストの結果(線形、1.0が白)をHDRのシーンのRTへ、UI(スプライト・ImGui)をUIのRT(SDR)へ描き、
	//   フレームの最後に1枚に合成してPQに変換する(UIの色がHDRで変わらないように)。
	//   SetBackBuffer()は、HDR出力のときはUIのRTを束縛する
	//------------------------------------------
	bool IsHDROutputSupported() const { return m_isHDRSupported; }
	bool IsHDROutput() const { return m_isHDROutput; }
	// HDR出力を切り替える。次のフレームの先頭でスワップチェーンを作り直す(対応していなければ何もしない)
	void RequestHDROutput(bool isEnabled);
	bool IsHDROutputRequested() const { return m_requestedHDROutput; }
	// モニターの最大の輝度(nit)。HDRのトーンマップの上限に使う
	float GetDisplayMaxLuminance() const { return m_displayMaxLuminance; }
	// 白(UIやSDRの1.0)を何nitで表示するか
	void SetPaperWhiteNits(float nits) { m_paperWhiteNits = (std::max)(nits, 1.0f); }
	float GetPaperWhiteNits() const { return m_paperWhiteNits; }
	// ポストの結果を描く先を束縛する(SDRならバックバッファ、HDR出力ならHDRのシーンのRT)
	void SetSceneOutput();
	void ClearSceneOutput(float r, float g, float b, float a);


	
	std::unique_ptr<QueueManager> m_upQueueManager = nullptr;
	std::unique_ptr<FrameManager> m_upFrameManager = nullptr;
	std::unique_ptr<ContextManager> m_upContextManager = nullptr;
	std::unique_ptr<DescriptorHeapManager> m_upDescriptorHeapManager = nullptr;

	std::unique_ptr<DepthStencil>			m_upDepthStencil = nullptr;
	std::unique_ptr<DepthStencil>			m_upShadowMap = nullptr;
	std::unique_ptr<ShadowMapArray>			m_upSpotShadowMaps = nullptr;
	std::unique_ptr<ShadowMapArray>			m_upCubeShadowMaps = nullptr;
	std::unique_ptr<ResourceStateTracker> m_upResourceStateTracker = nullptr;
	std::unique_ptr<ResourceUploader> m_upResourceUploader = nullptr;
	std::unique_ptr<ResourceLifetimeManager> m_upResourceLifetimeManager = nullptr;
		std::unique_ptr<Texture> m_spWhiteTex = nullptr;
	std::unique_ptr<Texture> m_spBlackTex = nullptr;
	std::unique_ptr<Texture> m_spNormalTex = nullptr;

	// ImGui用SRVヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>			m_upImGuiSRVHeap = nullptr;
	std::unique_ptr<DirectX::GraphicsMemory> m_graphicsMemory = nullptr;
	float GetVRAMUsageMB();
private:
	bool CreateFactory();
	bool CreateDevice();
				bool CreateSwapChain(HWND hWnd, int width, int height);
	bool CreateSwapChainRTV();
		bool CreateDefaultTextures();

	// 次のフレームを始めてよい時まで待つ(fpsの上限と、表示待ちのフレーム数の上限)。
	// 入力を読む前に待つことで、待った時間が入力の遅延にならないようにする
	void WaitForNextFrame();

	UINT m_backBufferWidth = 0;
	UINT m_backBufferHeight = 0;
	UINT m_pendingWidth = 0;
	UINT m_pendingHeight = 0;

	// HDR出力
	void UpdateHDRSupport();
	void ResizeHDRTargets(UINT width, UINT height);
	// HDRのシーンとUIを合成して、PQでスワップチェーンへ書く
	void CompositeHDROutput();
	bool m_isHDRSupported = false;
	bool m_isHDROutput = false;
	bool m_requestedHDROutput = false;
	bool m_isHDRChangePending = false;
	float m_displayMaxLuminance = 1000.0f;
	float m_paperWhiteNits = 200.0f;
	std::unique_ptr<RenderTarget> m_upHDRSceneTarget;
	std::unique_ptr<RenderTarget> m_upUITarget;

	FrameRateMode m_frameRateMode = FrameRateMode::Unlimited;
	int m_frameRateLimit = 144;
	std::chrono::steady_clock::time_point m_nextFrameTime = {};
	HANDLE m_frameLatencyWaitable = nullptr;
	HANDLE m_frameLimiterTimer = nullptr;

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
	static inline bool s_debugLayerActive = false;
public:

};







