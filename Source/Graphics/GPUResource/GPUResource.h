#pragma once

class GPUResource
{
public:
	GPUResource(){}
	virtual ~GPUResource(){}

public:
	GraphicsDevice* m_device = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_resource = nullptr;

	ID3D12Resource* GetResource() const { return m_resource.Get(); }
};
