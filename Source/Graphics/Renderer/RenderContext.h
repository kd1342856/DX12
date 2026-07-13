#pragma once
#include "../Buffer/CBufferAllocator/CBufferData/CBufferData.h"

struct RenderContext
{
	Math::Matrix View;
	Math::Matrix Projection;

	Math::Matrix LightView;
	Math::Matrix LightProjection;
	CBufferData::Light Light;

	void BindCamera(int slot = 0);
	void BindLight(int slot);
	// void BindShadowMap(int slot);
};




