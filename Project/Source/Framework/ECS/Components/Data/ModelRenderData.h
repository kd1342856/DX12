#pragma once

enum class ModelType {
    Static,
    Dynamic,
    Sky
};

// =============================================
// ModelRenderData
// 3Dモデルデータの保持
// =============================================
struct ModelRenderData
{
	std::shared_ptr<ModelData> m_spModelData;
	std::string m_filePath;
	ModelType m_modelType = ModelType::Static;

	// falseの間はRenderSystemが描画をスキップする(影・反射も含む)。
	// Ghostの徘徊中の非表示切り替えなど、実行時のみの状態でシーンには保存しない。
	bool m_isVisible = true;

	// 実行時のみ(保存しない)。falseなら影を落とさない(一人称のプレイヤーの体や手に持った物。
	// 手元から出る懐中電灯の光をさえぎってしまうため)
	bool m_castShadow = true;

	// Runtime-only (not saved) emissive override, used to make light fixtures glow (picked up by Bloom).
	// Set every frame by SpotLightSystem (from SpotLightData on the same entity) or by scripts (held flashlight).
	// m_glowColor is HDR (color * strength); zero = no override. m_glowMaterial: empty = every mesh,
	// otherwise only meshes whose material name matches exactly (e.g. "Glass" for the flashlight lens).
	Math::Vector3 m_glowColor = { 0.0f, 0.0f, 0.0f };
	std::string m_glowMaterial;
	// Optional second glowing material lit "by reflection" (e.g. the inside of a pendant lamp's shade):
	// it glows at m_glowColor * m_glowReflectScale on its back faces only. Empty/0 = none.
	std::string m_glowReflectMaterial;
	float m_glowReflectScale = 0.0f;

	// Runtime-only (not saved) bottom-up dissolve, e.g. an exorcised ghost vanishing from its feet.
	// While enabled, pixels whose world Y is below m_dissolveHeight are clipped and a glowing band
	// (m_dissolveEdgeColor, HDR) is drawn just above the cut. Skinned (Dynamic) models only for now.
	bool m_dissolveEnabled = false;
	float m_dissolveHeight = 0.0f;
	float m_dissolveEdgeWidth = 0.15f;
	Math::Vector3 m_dissolveEdgeColor = { 0.0f, 0.0f, 0.0f };

	// Per-instance node animation. Node poses normally live on the shared ModelData (cached per file path),
	// so every entity using the same model shows the same pose. When true (saved as "PerInstanceAnimation"),
	// AnimationSystem keeps this entity's own node local transforms / anim deltas below and the renderer
	// uses those instead (e.g. several wall switches toggling independently). Static (node) animation only.
	bool m_perInstanceAnimation = false;
	std::vector<Math::Matrix> m_instanceNodeLocal; // runtime only
	std::vector<Math::Matrix> m_instanceNodeDelta; // runtime only (Identity = rest pose)

	// The anim delta to apply to nodes[nodeIndex] when drawing this entity.
	const Math::Matrix& GetNodeAnimDelta(size_t nodeIndex, const ModelData::Node& node) const
	{
		if (!m_perInstanceAnimation) return node.animDeltaTransform;
		return nodeIndex < m_instanceNodeDelta.size() ? m_instanceNodeDelta[nodeIndex] : Math::Matrix::Identity;
	}
};
