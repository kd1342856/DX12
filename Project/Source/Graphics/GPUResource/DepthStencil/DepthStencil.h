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
	// 大きさだけを変えて作り直す。ディスクリプタの番号は変わらない。GPUの処理が終わってから呼ぶこと
	bool Resize(const Math::Vector2& resolution);

	void ClearBuffer();

	// リソースバリア遷移（状態管理付き）

	UINT GetDSVNumber() const { return m_dsvNumber; }
	int GetSRVNumber() const { return m_srvNumber; }
	ID3D12Resource* GetBuffer() const { return m_resource.Get(); }

private:
	bool CreateResource(const Math::Vector2& resolution);
	void WriteDescriptors();

	DepthStencilFormat m_format = DepthStencilFormat::DepthHighQuality;
	DXGI_FORMAT m_dsvFormat = DXGI_FORMAT_D32_FLOAT;
	bool m_hasSRV = false;
	UINT m_dsvNumber = 0;
	int m_srvNumber = -1;

	// 現在のリソースステート（Create直後はDEPTH_WRITE）
};