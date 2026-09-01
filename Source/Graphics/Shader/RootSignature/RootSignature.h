#pragma once
#include <vector>
#include <wrl.h>
#include <d3d12.h>
#include "../../../Graphics/Device/GraphicsDevice.h"

enum class RangeType {
	CBV,
	SRV,
	UAV,
};

struct DescriptorRange {
	RangeType Type;
	int Register;
	int Count;
	int Space = 0;
};

enum class TextureAddressMode { Wrap, Clamp };
enum class D3D12Filter { Point, Linear };

class RootSignature {
public:
	void Create(GraphicsDevice* pGraphicsDevice, const std::vector<DescriptorRange>& ranges);
	ID3D12RootSignature* GetRootSignature() { return m_pRootSignature.Get(); }
private:
	void CreateStaticSampler(D3D12_STATIC_SAMPLER_DESC& pSamplerDesc, TextureAddressMode mode, D3D12Filter filter, int count);
	GraphicsDevice* m_pDevice = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> m_pRootBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_pRootSignature = nullptr;
};