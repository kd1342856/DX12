#pragma once
enum class DepthStencilFormat
{
	DepthLowQuality = DXGI_FORMAT_R16_TYPELESS,
	DepthHighQuality = DXGI_FORMAT_R32_TYPELESS,
	DepthHighQualityAndStencil = DXGI_FORMAT_R24G8_TYPELESS,
};

class DepthStencil : public Buffer
{
public:
	bool Create(GraphicsDevice* pGraphicsDevice, const Math::Vector2& resolution, DepthStencilFormat format = DepthStencilFormat::DepthHighQuality, bool bCreateSRV = false);

	void ClearBuffer();

	// リソースバリア遷移（状態管理付き）
	void TransitionTo(ID3D12GraphicsCommandList* pCmdList, D3D12_RESOURCE_STATES newState);

	UINT GetDSVNumber() const { return m_dsvNumber; }
	int GetSRVNumber() const { return m_srvNumber; }
	ID3D12Resource* GetBuffer() const { return m_pBuffer.Get(); }

private:
	UINT m_dsvNumber = 0;
	int m_srvNumber = -1;

	// 現在のリソース状態（Create時はDEPTH_WRITE）
	D3D12_RESOURCE_STATES m_currentState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
};