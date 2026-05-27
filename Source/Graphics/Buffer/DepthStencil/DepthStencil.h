#pragma once
enum class DepthStencilFormat
{
	DepthLowQuality = DXGI_FORMAT_R16_TYPELESS,
	DepthHighQuality = DXGI_FORMAT_R32_TYPELESS,
	DepthHighQualityAndStencil = DXGI_FORMAT_R24G8_TYPELESS,
};

class DepthStencil : Buffer
{
public:
	bool Create(GraphicsDevice* pGraphicsDevice, const Math::Vector2& resolution, DepthStencilFormat format = DepthStencilFormat::DepthHighQuality, bool bCreateSRV = false);

	void ClearBuffer();

	UINT GetDSVNumber() const { return m_dsvNumber; }
	int GetSRVNumber() const { return m_srvNumber; }
private:
	UINT m_dsvNumber = 0;
	int m_srvNumber = -1;
};
