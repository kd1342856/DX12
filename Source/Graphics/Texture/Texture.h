#pragma once

class Texture
{
public:
	//	テクスチャのロード
	bool Load(GraphicsDevice* pGraphicsDevice, const std::string& filePath);

	//	シェーダーリソースとしてセット
	void Set(int index);

	int GetSRVNumber() { return m_srvNumber; }

private:
	GraphicsDevice* m_pDevice = nullptr;

	ComPtr<ID3D12Resource> m_pBuffer = nullptr;
	int m_srvNumber = 0;
};
