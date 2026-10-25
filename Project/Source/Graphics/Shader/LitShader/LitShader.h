#pragma once
#include "../GraphicsShader/GraphicsShader.h"

class LitShader : public GraphicsShader {
public:
	virtual void Create(GraphicsDevice* pGraphicsDevice) override;
	virtual void Begin(RenderContext& context) override;
	virtual void BeginNode(const ModelData::Node& node, const Math::Matrix& nodeWorld) override;
	virtual void BeforeDrawMesh(const Mesh& mesh, const Material& material) override;

	// 深度プリパス(RenderSystem::RenderNormalPrepass)で深度を描くマテリアルか。
	// 不透明(アルファテスト・半透明でない)で、プロシージャル(ディゾルブなどのclip)でないもの
	static bool IsDepthPrepassMaterial(const Material& material)
	{
		return material.Constants.alphaMode == 0 && material.Constants.proceduralType == 0;
	}
	// trueの間、不透明のメッシュをプリパスの深度とEQUALで描く(深度は書かない)。次のBeforeDrawMeshから効く
	void SetDepthEqual(bool isDepthEqual) { m_isDepthEqual = isDepthEqual; }

private:
	void SetMaterial(const Material& material);
	int m_cbvCount = 0;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_psoOpaque;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_psoOpaqueDepthEqual;
	bool m_isDepthEqual = false;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_psoMask;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_psoBlend;
};
