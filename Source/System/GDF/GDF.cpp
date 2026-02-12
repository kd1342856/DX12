#include "GDF.h"

// デバイス初期化
bool GDF::Init(HWND hwnd, int width, int height)
{
	return GraphicsDevice::Instance().Init(hwnd, width, height);
}

// フレーム開始（ヒープセット・定数バッファリセット込み）
void GDF::BeginFrame()
{
	GraphicsDevice::Instance().BeginFrame();
	GraphicsDevice::Instance().GetCBVSRVUAVHeap()->SetHeap();
	GraphicsDevice::Instance().GetCBufferAllocator()->ResetCurrentUseNumber();
}

// フレーム終了
void GDF::EndFrame()
{
	GraphicsDevice::Instance().EndFrame();
}

// 終了処理
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
