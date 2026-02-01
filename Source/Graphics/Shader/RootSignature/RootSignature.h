#pragma once

enum class RangeType
{
	CBV,
	SRV,
	UAV,
};

enum class TextureAddressMode
{
	Wrap,
	Clamp,
};

enum class D3D12Filter
{
	Point,
	Linear,
};

class RootSignature
{
public:
	void Create(GraphicsDevice* pGraphicsDevice, const std::vector<RangeType>& rangeTypes);

	ID3D12RootSignature* GetRootSignature() { return m_pRootSignature.Get(); }


private:
	void CreateRange(D3D12_DESCRIPTOR_RANGE& pRange, RangeType type, int count);

	void CreateStaticSampler(D3D12_STATIC_SAMPLER_DESC& pSamplerDesc, TextureAddressMode mode, D3D12Filter filter, int count);

	GraphicsDevice*				m_pDevice = nullptr;
	ComPtr<ID3DBlob>			m_pRootBlob = nullptr;
	ComPtr<ID3D12RootSignature> m_pRootSignature = nullptr;
};
