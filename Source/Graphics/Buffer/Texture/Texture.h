#pragma once

class Texture : public Buffer
{
public:
	//	テクスチャのロード
	bool Load(GraphicsDevice* pGraphicsDevice, const std::string& filePath);

	//	シェーダーリソースとしてセット
	void Set(int index);

	int GetSRVNumber() { return m_srvNumber; }

private:
	int m_srvNumber = 0;
};
