#pragma once
#include <cstdint>
struct MeshVertex
{
	Math::Vector3 Position;
	Math::Vector2 UV;
	Math::Vector3 Normal;
	UINT Color = 0xFFFFFFFF;
	Math::Vector3 Tangent;
	uint16_t SkinIndex[4] = { 0, 0, 0, 0 };
	float SkinWeight[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
};
