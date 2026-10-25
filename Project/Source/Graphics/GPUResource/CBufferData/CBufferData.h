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

		// 平面反射(窓ガラス等)用: 各反射スロット(最大3枚=3窓まで同時)の反射カメラView*Proj行列。
		// ガラスのピクセルシェーダーがワールド座標をこれで再投影して
		// g_planarReflectionMapN(N=スロット番号)の正しいUVを求めるために使う。
		Math::Matrix mReflectionVP[3];
		DirectX::XMINT4 HasReflectionPacked; // x,y,z = スロット0/1/2の有効フラグ(0/1)、wは未使用
	};

	struct SpotLight
	{
		Math::Vector3 Dir;
		float Range;
		Math::Vector3 Color;
		float InnerCorn;
		Math::Vector3 Pos;
		float OuterCorn;
		// 影(LocalShadowManagerが割り当てる)。番号は-1なら影なし
		float ShadowIndex;     // 2D配列の枚の番号(狭いスポット)
		float ShadowBias;      // 影のバイアスの倍率(1で標準)
		float ShadowCubeIndex; // キューブ配列の個の番号(広いスポット)
		float SourceRadius;    // 光源の半径(m)。PCSSの半影の広さに使う
		Math::Matrix mLightVP; // 2D配列の影の行列
	};

	struct PointLight
	{
		Math::Vector3 Pos;
		float Range;
		Math::Vector3 Color;
		float ShadowCubeIndex; // キューブ配列の個の番号。-1なら影なし
		float SourceRadius;    // 光源の半径(m)。PCSSの半影の広さに使う
		float ShadowBias;      // 影のバイアスの倍率(1で標準)
		float pad[2];
	};

	struct Light
	{
		int SL_Count;
		// ダミーIBLと空の明るさ(nitの値に露出を掛けたもの)
		float IBLLuminance;
		float SkyLuminance;
		float dummy1;
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

		// 室内判定(RoomAreaのAABB、xyzのみ使用)。範囲内のピクセルは外の光(月明かり・環境光・
		// 空の反射光)を受けず、月の影の中と同じ扱いになる(GameScene::UpdateLightsが毎フレーム更新)。
		int32_t IndoorCount;
		float PadIndoor[3];
		Math::Vector4 IndoorMin[16];
		Math::Vector4 IndoorMax[16];

		// 反射プローブ(ReflectionProbesが毎フレーム書く)。箱はxyzだけを使う
		int32_t ProbeCount;
		float ProbeMipCount;
		float PadProbe[2];
		Math::Vector4 ProbeBoxMin[16];
		Math::Vector4 ProbeBoxMax[16];
		Math::Vector4 ProbeCapturePosition[16]; // xyz: 撮った位置、w: 露出の比(今の露出 ÷ 撮ったときの露出)。0なら未撮影

		// DDGI(DDGIVolumeが毎フレーム書く)
		Math::Vector4 DDGIOrigin;    // xyz: 最初のプローブの位置、w: プローブの間隔(m)
		DirectX::XMINT4 DDGICounts;  // xyz: 各軸のプローブの数、w: テクスチャの1行に並べるプローブの数
		Math::Vector4 DDGIParams;    // x: 有効(1)か、y: 法線の向きへずらす量、z: カメラの向きへずらす量
	};

	struct SSAO
	{
		float Radius;
		float Bias;
		float Power;
		float Intensity;
	};

	struct SSROpaque
	{
		float StepSize;
		float Intensity;
		float Pad[2];
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
		float BrightnessGamma;
	};
}