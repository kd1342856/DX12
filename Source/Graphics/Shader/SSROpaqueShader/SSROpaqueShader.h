#pragma once
#include "../GraphicsShader/GraphicsShader.h"

class RenderTarget;

// SSR(Opaque材質向け)。専用のG-Bufferパスを増やさず、NormalPrepass(SSAO用)の法線+深度と、
// Opaqueパスのカラーコピー(既存の屈折用バッファ)を再利用する。結果はpDestRT(通常はシーンHDR
// そのもの)へ加算合成する(BlendMode::Add)ので、呼び出し側はpDestRTがRENDER_TARGET状態に
// なっていることを保証すること。
class SSROpaqueShader : public GraphicsShader
{
public:
	virtual void Create(GraphicsDevice* pGraphicsDevice) override;

	struct Params
	{
		float StepSize = 0.35f;
		float Intensity = 0.5f;
	};

	void Draw(RenderTarget* pNormalRT, RenderTarget* pSceneOpaqueCopy, RenderTarget* pDestRT, const Params& params);
};
