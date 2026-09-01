#pragma once
enum class DepthStencilFormat
{
	DepthLowQuality = DXGI_FORMAT_R16_TYPELESS,
	DepthHighQuality = DXGI_FORMAT_R32_TYPELESS,
	DepthHighQualityAndStencil = DXGI_FORMAT_R24G8_TYPELESS,
};

class DepthStencil : public GPUResource
{
public:
	bool Create(GraphicsDevice* pGraphicsDevice, const Math::Vector2& resolution, DepthStencilFormat format = DepthStencilFormat::DepthHighQuality, bool bCreateSRV = false);

	void ClearBuffer();

	// ???\?[?X?o???A?J??i??????t???j

	UINT GetDSVNumber() const { return m_dsvNumber; }
	int GetSRVNumber() const { return m_srvNumber; }
	ID3D12Resource* GetBuffer() const { return m_resource.Get(); }

private:
	UINT m_dsvNumber = 0;
	int m_srvNumber = -1;

	// ???????\?[?X???iCreate????DEPTH_WRITE?j
};