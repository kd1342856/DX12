#pragma once

class Texture : public Buffer
{
public:
	//	テクスチャのロード
	bool Load(GraphicsDevice* pGraphicsDevice, const std::string& filePath);

	// メモリからテクスチャを作成
	bool CreateFromMemory(const void* data, int width, int height, DXGI_FORMAT format);

	//	シェーダーリソースとしてセット
	void Set(int index);

	int GetSRVNumber() { return m_srvNumber; }

public:
	int m_srvNumber = 0;
private:
};
