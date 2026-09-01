#pragma once

// =============================================
// TransformData
// 位置・回転・スケール・ワールド行列
// =============================================
struct TransformData
{
	Math::Vector3 m_position = { 0, 0, 0 };
	Math::Vector3 m_rotation = { 0, 0, 0 };
	Math::Vector3 m_scale	 = { 1, 1, 1 };
	Math::Matrix  m_worldMatrix;
};
