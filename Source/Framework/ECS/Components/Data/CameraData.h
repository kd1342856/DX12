#pragma once

enum class CameraMode {
	EditorFree = 0,
	TPS,
	FPS
};

struct CameraData
{
	Math::Matrix m_viewMatrix;
	Math::Matrix m_projMatrix;
	float m_fov = 60.0f;
	float m_nearZ = 0.01f;
	float m_farZ = 1000.0f;
	float m_moveSpeed = 0.1f;
	CameraMode m_cameraMode = CameraMode::EditorFree;
};