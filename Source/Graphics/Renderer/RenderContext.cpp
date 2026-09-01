#include "../../Pch.h"
#include "RenderContext.h"
#include "../GDF/GDF.h"
#include "../Shader/ShaderManager/ShaderManager.h"

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
	GDF::Instance().BindCBuffer(slot, ShaderManager::Instance().GetLightData());
}

void RenderContext::BindSystem(int slot)
{
	GDF::Instance().BindCBuffer(slot, ShaderManager::Instance().GetSystemData());
}
