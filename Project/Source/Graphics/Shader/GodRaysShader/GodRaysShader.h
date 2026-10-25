#pragma once
#include "../GraphicsShader/GraphicsShader.h"

class RenderTarget;

// God Rays(光条/Crepuscular Rays)。Bloom結果を光源からラジアルブラーして加算する。
class GodRaysShader : public GraphicsShader
{
public:
	virtual void Create(GraphicsDevice* pDevice) override;

	// pSrcRT: 光源を含む明るい部分のテクスチャ(Bloom結果の再利用を想定)
	void Draw(RenderTarget* pSrcRT, RenderTarget* pDestRT, const CBufferData::PostProcess& postProcessData);
};
