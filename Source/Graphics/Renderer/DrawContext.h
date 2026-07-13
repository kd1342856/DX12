#pragma once
#include <vector>

struct DrawContext
{
	Math::Matrix World;
	const std::vector<Math::Matrix>* BoneMatrices = nullptr;
	
	DrawContext& SetWorld(const Math::Matrix& world) {
		World = world;
		return *this;
	}

	DrawContext& SetBones(const std::vector<Math::Matrix>* bones) {
		BoneMatrices = bones;
		return *this;
	}

	// Future extensions:
	// DrawContext& SetEntity(Entity entity) { ... return *this; }
};

