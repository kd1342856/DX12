#pragma once

namespace CBufferData
{
	struct Camera
	{
		Math::Matrix mView;
		Math::Matrix mInvV;
		Math::Matrix mProj;
		Math::Matrix mInvP;
		Math::Matrix mVP;
		Math::Matrix mInvVP;
		Math::Vector3 CamPos;
		float dummy;
	};

	struct PerDraw
	{
		Math::Matrix mWorld;
	};

	struct Material
	{
		Math::Vector4 BaseColor;
		Math::Vector4 EmissiveColor;
		float Metallic;
		float Smoothness;
		float dummy[2];
	};

	struct Light
	{
		int SL_Count;
		Math::Vector3 dummy1;
		Math::Vector3 AmbientLight;
		float dummy2;
		Math::Vector3 DL_Dir;
		float DL_ShadowBias;
		Math::Vector3 DL_Color;
		float DL_ShadowPower;
		Math::Matrix DL_mLightVP[3];
		Math::Vector4 DL_CascadeSplits;
		// Spotlights etc. omitted for simplicity, but padded to match 16-byte alignment
		Math::Vector4 dummy3[10 * 6]; // SpotLight struct approx padding
		Math::Vector3 DistanceFogColor;
		float DistanceFogDensity;
	};

	struct PostProcess
	{
		float Exposure;
		float dummy[3];
	};
}