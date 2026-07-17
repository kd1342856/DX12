#pragma once

class Texture : public GPUResource
{
public:
	//	�e�N�X�`���̃��[�h
	bool Load(GraphicsDevice* pGraphicsDevice, const std::string& filePath);

	// ����������e�N�X�`����쐬
	bool CreateFromMemory(const void* data, int width, int height, DXGI_FORMAT format);

	//	�V�F�[�_�[���\�[�X�Ƃ��ăZ�b�g
	void Set(int index);

	int GetSRVNumber() { return m_srvNumber; }

public:
	int m_srvNumber = 0;
private:
};
