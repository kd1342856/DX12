#include "../../../Pch.h"
#include "GDF.h"


// デバイス初期化
bool GDF::Init(HWND hwnd, int width, int height)
{
	return GraphicsDevice::Instance().Init(hwnd, width, height);
}

// フレーム開始（ヒープセット・定数バッファリセット・ImGuiNewFrame）
void GDF::BeginFrame()
{
	GraphicsDevice::Instance().BeginFrame();
	GraphicsDevice::Instance().GetCBVSRVUAVHeap()->SetHeap();

	// Input状態を毎フレーム更新
	Input::Instance().Update();
	GraphicsDevice::Instance().GetCBufferAllocator()->ResetCurrentUseNumber();

	// ImGuiフレーム開始
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

// フレーム終了
void GDF::EndFrame()
{
	GraphicsDevice::Instance().EndFrame();
}

// 終了
void GDF::Shutdown()
{
	GraphicsDevice::Instance().Shutdown();
}

// 低レベルアクセス
GraphicsDevice& GDF::GetGraphicsDevice()
{
	return GraphicsDevice::Instance();
}

ID3D12Device8* GDF::GetDevice() const
{
	return GraphicsDevice::Instance().GetDevice();
}

ID3D12GraphicsCommandList6* GDF::GetCmdList() const
{
	return GraphicsDevice::Instance().GetCmdList();
}

Texture* GDF::GetWhiteTex()
{
	return GetGraphicsDevice().GetWhiteTex();
}

Texture* GDF::GetBlackTex()
{
	return GetGraphicsDevice().GetBlackTex();
}

Texture* GDF::GetNormalTex()
{
	return GetGraphicsDevice().GetNormalTex();
}

GDF& GDF::Instance()
{
    static GDF instance;
    return instance;
}
