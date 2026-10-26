#pragma once
#include "../GPUResource/CBufferData/CBufferData.h"
#include "../GPUResource/FrameConstantBufferAllocator/FrameConstantBufferAllocator.h"

struct RenderContext
{
	Math::Matrix View;
	Math::Matrix Projection;

	Math::Matrix LightView;
	Math::Matrix LightProjection;

	// メインのカメラの視点で、シーンのRTに描いているか。画面の深度を使う効果(コンタクトシャドウ)は、
	// 反射・反射プローブ・プレビューの描画では視点が合わないので使わない
	bool IsMainView = true;

	// 中身が前に束縛したものと同じなら、そのフレームで作った定数バッファを使い回す
	// (物を描くたびに呼ばれるので、毎回書き直すとフレームの定数バッファの容量を使い切る)
	void BindCamera(int slot = 0);
	void BindLight(int slot);
	void BindSystem(int slot);

private:
	ConstantBufferCache m_cameraCache;
	ConstantBufferCache m_lightCache;
	ConstantBufferCache m_systemCache;
};




