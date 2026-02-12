#pragma once

// =============================================
// CameraComponent
// カメラのビュー・プロジェクション情報
// =============================================
struct CameraComponent
{
	Math::Matrix m_viewMatrix;
	Math::Matrix m_projMatrix;
	float m_fov = 60.0f;
	float m_nearZ = 0.01f;
	float m_farZ = 1000.0f;
};
