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
	Math::Vector3 m_targetOffset = { 0.0f, 0.0f, -5.0f }; // TPS Base Offset
	Math::Vector3 m_fpsOffset = { 0.0f, 1.5f, 0.0f };     // FPS Base Offset

	// trueなら、このカメラのTransformで毎フレーム専用のプレビュー用RTに描画する
	// (メイン/エディタカメラや反射カメラとは独立)。「Preview Camera」ウィンドウで
	// 表示先を選ぶ代わりに、シーン上にこのフラグを立てたカメラを1つ置くだけで良い。
	bool m_isDebugPreview = false;
};