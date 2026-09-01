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

	struct Bones
	{
		Math::Matrix mBones[256];
	};

	struct PerDraw
	{
		Math::Matrix mWorld;
	};

	struct System
	{
		int DebugView;
		int EnableHDR;
		float Exposure;
		float Gamma;
		float ScreenWidth;
		float ScreenHeight;
		float pad[2];

		// 平面反射(窓ガラス等)用: 反射カメラのView*Proj行列。
		// ガラスのピクセルシェーダーがワールド座標を再投影して
		// g_planarReflectionMapの正しいUVを求めるために使う。
		Math::Matrix mReflectionVP;
		int HasReflection; // 今フレーム有効な反射があるか(0/1)
		float padReflection[3];
	};

	struct SpotLight
	{
		Math::Vector3 Dir;
		float Range;
		Math::Vector3 Color;
		float InnerCorn;
		Math::Vector3 Pos;
		float OuterCorn;
		float EnableShadow;
		float ShadowBias;
		float padding[2];
		Math::Matrix mLightVP;
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
		SpotLight SL[10];
		Math::Vector3 DistanceFogColor;
		float DistanceFogDensity;
	};

	struct PostProcess
	{
		float Exposure;
		float BloomThreshold;
		float BloomIntensity;
		float BlurDirectionX;
		float BlurDirectionY;
		float Gamma;
		uint32_t EnableHDR;
		float Pad[1];
	};
}