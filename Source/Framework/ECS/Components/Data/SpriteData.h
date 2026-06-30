#pragma once

#include <string>
#include <memory>
#include <SimpleMath.h>

class Texture;

struct SpriteData {
	// 画像ファイルパス
	std::string m_filePath = "";
	
	// キャッシュ済みのテクスチャポインタ
	std::shared_ptr<Texture> m_spTexture = nullptr;

	// 色（RGBA）
	Math::Vector4 m_color = { 1.0f, 1.0f, 1.0f, 1.0f };

	// サイズ（幅、高さ）
	Math::Vector2 m_size = { 100.0f, 100.0f };

	// ピボット(0.0~1.0, 0.5が中心)
	Math::Vector2 m_pivot = { 0.5f, 0.5f };

	// 描画順序（値が大きいほど手前に描画）
	int m_orderInLayer = 0;
};
