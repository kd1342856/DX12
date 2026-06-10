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
	Math::Vector3 m_targetOffset = { 0.0f, 2.0f, -5.0f }; // TPS Base Offset
	Math::Vector3 m_fpsOffset = { 0.0f, 1.5f, 0.0f };     // FPS Base Offset
};