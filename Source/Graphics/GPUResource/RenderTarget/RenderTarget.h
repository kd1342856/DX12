#pragma once
#include "../GPUResource.h"
class RenderTarget : public GPUResource
{
public:
	RenderTarget() {}
	~RenderTarget() {}

	bool Create(int width, int height, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM);

	int GetWidth() const { return m_width; }
	int GetHeight() const { return m_height; }

	int GetRTVIndex() const			{ return m_rtvIndex; }
	int GetSRVIndex() const			{ return m_srvIndex; }
	int GetImGuiSRVIndex() const	{ return m_imGuiSrvIndex; }
	int GetDSVIndex() const			{ return m_dsvIndex; }
	// このRenderTarget専用の深度バッファをシェーダーから読むためのSRV
	// (DOF/SSAO/SSR等、"このRTに実際に書き込まれた深度"を後段パスで参照する用途)。
	int GetDepthSRVIndex() const		{ return m_depthSrvIndex; }
	ID3D12Resource* GetDepthResource() const { return m_pDepthBuffer.Get(); }
	ID3D12Resource* GetResource() const { return m_resource.Get(); }

	void Clear(float r = 0.0f, float g = 0.0f, float b = 1.0f, float a = 1.0f);

private:
	int m_width = 0;
	int m_height = 0;

	int m_rtvIndex		= -1;
	int m_srvIndex		= -1;
	int m_imGuiSrvIndex = -1;
	int m_dsvIndex		= -1;
	int m_depthSrvIndex = -1;

	Microsoft::WRL::ComPtr<ID3D12Resource> m_pDepthBuffer;
};