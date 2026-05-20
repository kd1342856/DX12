#pragma once

class Texture : public Buffer
{
public:
	//	テクスチャのロード
	bool Load(GraphicsDevice* pGraphicsDevice, const std::string& filePath);

	//	シェーダーリソースとしてセット
	void Set(int index);

	int GetSRVNumber() { return m_srvNumber; }

public:
	ComPtr<ID3D12Resource> m_pBuffer;
	GraphicsDevice* m_pGraphicsDevice;
public:
	int m_srvNumber = 0;
private:
};
