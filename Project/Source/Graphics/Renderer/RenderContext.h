#pragma once
#include "../GPUResource/CBufferData/CBufferData.h"

struct RenderContext
{
	Math::Matrix View;
	Math::Matrix Projection;

	Math::Matrix LightView;
	Math::Matrix LightProjection;

	// メインのカメラの視点で、シーンのRTに描いているか。画面の深度を使う効果(コンタクトシャドウ)は、
	// 反射・反射プローブ・プレビューの描画では視点が合わないので使わない
	bool IsMainView = true;

	void BindCamera(int slot = 0);
	void BindLight(int slot);
	void BindSystem(int slot);
};




