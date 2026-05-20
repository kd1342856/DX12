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
	bool Create(GraphicsDevice* pGraphicsDevice, const Math::Vector2& resolution, DepthStencilFormat format = DepthStencilFormat::DepthHighQuality);

	void ClearBuffer();

	UINT GetDSVNumber() { return m_dsvNumber; }
private:
	UINT m_dsvNumber = 0;
};
