#include "../../Pch.h"
#include "RenderContext.h"
#include "../GDF/GDF.h"

void RenderContext::BindCamera(int slot)
{
	CBufferData::Camera cbCamera;
	cbCamera.mView = View;
	cbCamera.mProj = Projection;
	cbCamera.CamPos = Math::Vector3::Transform(Math::Vector3::Zero, View.Invert());
	GDF::Instance().BindCBuffer(slot, cbCamera);
}

void RenderContext::BindLight(int slot)
{
	GDF::Instance().BindCBuffer(slot, Light);
}

// void RenderContext::BindShadowMap(int slot)
// {
// 
// }
