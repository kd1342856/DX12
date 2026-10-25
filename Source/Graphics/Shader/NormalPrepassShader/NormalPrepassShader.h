#pragma once
#include "../GraphicsShader/GraphicsShader.h"

// SSAO/SSR用のビュー空間法線プリパス。Opaque形状だけをもう一度描き直して
// (ShadowShaderと同じ「もう1周ジオメトリを流す」パターン)、法線をRTに書き出す。
// 深度は通常のZバッファ(RenderTargetが持つ専用DepthBuffer)をそのまま使う。
class NormalPrepassShader : public GraphicsShader
{
public:
    virtual void Create(GraphicsDevice* pGraphicsDevice) override;
    virtual void Begin(RenderContext& context) override;
    virtual void BeginNode(const ModelData::Node& node, const Math::Matrix& nodeWorld) override;
    // SSR用に粗さ(マテリアル定数+MetallicRoughnessテクスチャ)をバインドする
    virtual void BeforeDrawMesh(const Mesh& mesh, const Material& material) override;
};
