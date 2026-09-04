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

		// SSR (スクリーンスペース反射) - 平面反射が無いガラス等のフォールバックに使う
		int32_t EnableSSR;
		float SSRStepSize;

		// ���ʔ���(���K���X��)�p: ���˃J������View*Proj�s��B
		// �K���X�̃s�N�Z���V�F�[�_�[�����[���h���W���ē��e����
		// g_planarReflectionMap�̐�����UV�����߂邽�߂Ɏg���B
		Math::Matrix mReflectionVP;
		int HasReflection; // ���t���[���L���Ȕ��˂����邩(0/1)
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

	struct PointLight
	{
		Math::Vector3 Pos;
		float Range;
		Math::Vector3 Color;
		float pad;
	};

	struct Light
	{
		int SL_Count;
		Math::Vector3 dummy1;
		Math::Vector3 AmbientLight;
		float IndirectShadowFloor; // 影が落ちる場所に残す間接光(Ambient/IBL)の下限 0..1
		Math::Vector3 DL_Dir;
		float DL_ShadowBias;
		Math::Vector3 DL_Color;
		float DL_ShadowPower;
		Math::Matrix DL_mLightVP[3];
		Math::Vector4 DL_CascadeSplits;
		SpotLight SL[10];
		Math::Vector3 DistanceFogColor;
		float DistanceFogDensity;

		int PL_Count;
		Math::Vector3 dummyPL;
		PointLight PL[8]; // ロウソク/裸電球等、フリッカー付きの点光源(LightSystemが毎フレーム更新)

		// ポイント光の影(最も近い1灯のみ、簡易単一パースペクティブシャドウ)
		Math::Matrix PL0_ShadowVP;
		float PL0_ShadowBias;
		int32_t PL0_ShadowEnabled;
		float PadPL0Shadow[2];
	};

	struct SSAO
	{
		float Radius;
		float Bias;
		float Power;
		float Intensity;
	};

	struct PostProcess
	{
		// Tonemap / Exposure
		float Exposure;
		float Gamma;
		uint32_t EnableHDR;
		float Time;

		// Bloom
		float BloomThreshold;
		float BloomIntensity;
		float BlurDirectionX; // Blurパス専用
		float BlurDirectionY;

		float BlurRadius;
		// Vignette
		float VignetteIntensity;
		float VignetteSmoothness;
		// Film Grain
		float FilmGrainIntensity;

		// Chromatic Aberration
		float ChromaticAberrationIntensity;
		// Depth of Field
		uint32_t EnableDOF;
		float DOFFocusDistance;
		float DOFFocusRange;

		// Camera Near/Far (深度リニア化用)
		float CameraNear;
		float CameraFar;
		// God Rays: 光源のスクリーン空間UV
		float GodRaysLightU;
		float GodRaysLightV;

		// God Rays
		float GodRaysDensity;
		float GodRaysDecay;
		float GodRaysWeight;
		float GodRaysExposure;

		uint32_t GodRaysNumSamples;
		uint32_t EnableGodRays;
		float GodRaysIntensity;
		float Pad;
	};
}