//================================
//
// 動く物の速度(モーションベクトル)
//
//   カメラだけが動いたときの速度は、深度から求める(Temporal/CameraVelocity_CS.hlsl)。
//   ここでは、前のフレームから動いた物(スキニングのモデル、開閉する扉など)だけを描き直し、
//   その上に物自体の動きを含んだ速度を書く。
//   速度 = 今のフレームの画面のUV - 前のフレームの画面のUV(どちらもジッターを含まない行列で求める)
//
//================================

// カメラ(ジッターを含む。ラスタライズの位置をメインの描画と1ビットも違わないようにする)
cbuffer cbCamera : register(b0)
{
    row_major float4x4 g_mV;
    row_major float4x4 g_mInvV;
    row_major float4x4 g_mP;
    row_major float4x4 g_mInvP;
    row_major float4x4 g_mVP;
    row_major float4x4 g_mInvVP;
    float3 g_CamPos;
    float g_cameraDummy;
};

// 今のフレームのワールド行列
cbuffer cbWorld : register(b1)
{
    row_major float4x4 g_mW;
};

// スキニングの骨(今のフレーム)
cbuffer cbBones : register(b2)
{
    row_major float4x4 g_mBones[256];
};

// 前のフレームのワールド行列
cbuffer cbPrevWorld : register(b3)
{
    row_major float4x4 g_mPrevW;
};

// ジッターを含まないビュー×投影行列(今と前のフレーム)
cbuffer cbVelocityFrame : register(b4)
{
    row_major float4x4 g_mCurrVPNoJitter;
    row_major float4x4 g_mPrevVPNoJitter;
};

// スキニングの骨(前のフレーム)
cbuffer cbPrevBones : register(b5)
{
    row_major float4x4 g_mPrevBones[256];
};

// マテリアル(アルファテストにだけ使う。並びはC++のMaterialConstantsと同じ)
cbuffer cbPerMaterial : register(b6)
{
    float4 g_baseColorFactor;
    float g_metallicFactor;
    float g_roughnessFactor;
    float g_normalScale;
    float g_occlusionStrength;
    float g_emissiveStrength;
    float3 g_emissiveFactor;
    int g_alphaMode; // 0:Opaque, 1:Mask, 2:Blend
};

Texture2D g_baseMap : register(t0);
SamplerState g_ss_linear_wrap : register(s2);

struct VelocityVSOutput
{
    float4 Pos : SV_Position;
    float4 CurrClip : TEXCOORD0;
    float4 PrevClip : TEXCOORD1;
    float2 UV : TEXCOORD2;
};

// 投影座標から画面のUVへ
float2 ClipToUV(float4 clipPos)
{
    float2 ndc = clipPos.xy / clipPos.w;
    return ndc * float2(0.5, -0.5) + 0.5;
}
