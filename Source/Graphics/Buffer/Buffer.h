#pragma once

class Buffer
{
public:
	Buffer(){}
	virtual ~Buffer(){}

public:
	GraphicsDevice* m_pGraphicsDevice = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_pBuffer = nullptr;
};
