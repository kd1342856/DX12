#pragma once
#include "../GraphicsShader/GraphicsShader.h"

class RenderTarget;

// SSAO(Screen Space Ambient Occlusion)。NormalPrepassの法線+深度を使う半球サンプリング版。
class SSAOShader : public GraphicsShader
{
public:
	virtual void Create(GraphicsDevice* pGraphicsDevice) override;

	struct Params
	{
		float Radius = 0.5f;
		float Bias = 0.03f;
		float Power = 1.5f;
		float Intensity = 1.0f;
	};

	// pNormalRT: NormalPrepassShaderで書き出した法線+深度のRT。RenderContextの
	// カメラ行列(Renderer::GetContext())を使ってビュー空間へ変換するので、
	// 呼び出し前にcontext.View/Projectionが正しいカメラのものになっていること。
	void Draw(RenderTarget* pNormalRT, RenderTarget* pDestRT, const Params& params);
};
