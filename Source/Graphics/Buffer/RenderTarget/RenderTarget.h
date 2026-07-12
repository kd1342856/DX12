#pragma once

class RenderTarget : public Buffer
{
public:
	RenderTarget() {}
	~RenderTarget() {}

	bool Create(int width, int height);

	int GetRTVIndex() const			{ return m_rtvIndex; }
	int GetSRVIndex() const			{ return m_srvIndex; }
	int GetImGuiSRVIndex() const	{ return m_imGuiSrvIndex; }
	int GetDSVIndex() const			{ return m_dsvIndex; }
	ID3D12Resource* GetResource() const { return m_pBuffer.Get(); }

	void Clear(float r = 0.0f, float g = 0.0f, float b = 1.0f, float a = 1.0f);
	void TransitionToRenderTarget();
	void TransitionToShaderResource();

private:
	int m_rtvIndex		= -1;
	int m_srvIndex		= -1;
	int m_imGuiSrvIndex = -1;
	int m_dsvIndex		= -1;

	// Zバッファ用
	Microsoft::WRL::ComPtr<ID3D12Resource> m_pDepthBuffer;
};