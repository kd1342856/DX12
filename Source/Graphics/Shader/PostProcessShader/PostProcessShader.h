#pragma once
#include "../GraphicsShader/GraphicsShader.h"

class RenderTarget;

class PostProcessShader : public GraphicsShader
{
public:
	virtual void Create(GraphicsDevice* pGraphicsDevice) override;

	// pDofRT: 被写界深度用にあらかじめぼかしておいたシーンのコピー(nullptrならDOF無効時と同じ扱い)。
	// シーン深度はpSceneRT自身が持つ専用深度バッファ(RenderTarget::GetDepthSRVIndex)を使う
	// (DOFのCoC計算用。呼び出し側はpSceneRTの深度をあらかじめSRVへ遷移させておくこと)。
	// pGodRaysRT: God Rays(光条)の結果(nullptrなら無効時と同じ扱い)。
	void Draw(RenderTarget* pSceneRT, RenderTarget* pBloomRT, const CBufferData::PostProcess& postProcessData,
		RenderTarget* pDofRT = nullptr, RenderTarget* pGodRaysRT = nullptr);
};
