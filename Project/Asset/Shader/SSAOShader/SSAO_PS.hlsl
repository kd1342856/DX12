#include "../Common/ShaderCore.hlsli"

// SSAO専用パラメータ (b0=カメラ, b1=PerDrawはShaderCore.hlsliで使用済みのためb2を使う)
cbuffer cbSSAO : register(b2)
{
    float g_SSAORadius;
    float g_SSAOBias;
    float g_SSAOPower;
    float g_SSAOIntensity;
};

Texture2D g_texNormal : register(t0); // NormalPrepassのビュー空間法線(0..1にパック済み)
Texture2D g_texDepth : register(t1);  // NormalPrepassの深度(このRT自身の専用DepthBuffer)
// サンプラーはShaderCore.hlsliのg_ss_point_clamp(s5)を再利用する

struct PSInput
{
    float4 Pos : SV_Position;
    float2 UV : TEXCOORD0;
};

// 12方向の半球サンプルカーネル(あらかじめ正規化・ランダムな長さで分布させたもの)。
// ランタイムで乱数生成する代わりに、決め打ちの方向セットを使う軽量版SSAO。
static const float3 kKernel[12] =
{
    float3(0.070, 0.045, 0.045), float3(-0.060, 0.080, 0.060),
    float3(0.090, -0.050, 0.090), float3(-0.080, -0.070, 0.050),
    float3(0.030, 0.140, 0.080), float3(-0.030, -0.140, 0.070),
    float3(0.160, 0.020, 0.030), float3(-0.160, 0.030, 0.050),
    float3(0.050, 0.060, 0.220), float3(-0.050, -0.060, 0.200),
    float3(0.190, -0.120, 0.140), float3(-0.190, 0.110, 0.160),
};

float3 ViewPosFromDepth(float2 uv, float depth)
{
    float2 ndc = uv * float2(2.0, -2.0) + float2(-1.0, 1.0);
    float4 clip = float4(ndc, depth, 1.0);
    float4 viewPos = mul(clip, g_mInvP);
    return viewPos.xyz / viewPos.w;
}

// ピクセルごとに向きを変えるための簡易ハッシュ回転(ノイズテクスチャ不要版)
float3 RandomRotation(float2 screenPos)
{
    float a = frac(sin(dot(screenPos, float2(12.9898, 78.233))) * 43758.5453) * 6.28318530718;
    return float3(cos(a), sin(a), 0.0);
}

float4 main(PSInput In) : SV_Target0
{
    float depth = g_texDepth.Sample(g_ss_point_clamp, In.UV).r;
    if (depth >= 0.9999) return float4(1, 1, 1, 1); // far平面(何も無い所)は遮蔽なし

    float3 viewPos = ViewPosFromDepth(In.UV, depth);
    float3 viewNormal = normalize(g_texNormal.Sample(g_ss_point_clamp, In.UV).rgb * 2.0 - 1.0);

    float3 rvec = RandomRotation(In.Pos.xy);
    float3 tangent = normalize(rvec - viewNormal * dot(rvec, viewNormal));
    float3 bitangent = cross(viewNormal, tangent);
    float3x3 TBN = float3x3(tangent, bitangent, viewNormal);

    float occlusion = 0.0;
    [unroll]
    for (int i = 0; i < 12; i++)
    {
        float3 samplePos = viewPos + mul(kKernel[i], TBN) * g_SSAORadius;

        float4 offset = mul(float4(samplePos, 1.0), g_mP);
        offset.xyz /= offset.w;
        float2 sampleUV = offset.xy * float2(0.5, -0.5) + 0.5;

        if (any(sampleUV < 0.0) || any(sampleUV > 1.0)) continue;

        float sampleDepthNdc = g_texDepth.Sample(g_ss_point_clamp, sampleUV).r;
        float3 sampleViewPos = ViewPosFromDepth(sampleUV, sampleDepthNdc);

        float rangeCheck = saturate(g_SSAORadius / max(abs(viewPos.z - sampleViewPos.z), 0.0001));
        // 左手系: 視点からの距離はZが大きいほど遠い。実際の表面(sampleViewPos.z)が
        // 理論上のサンプル点(samplePos.z)より手前(値が小さい)にあれば、その間に
        // 遮蔽物があるということ。
        occlusion += (sampleViewPos.z <= samplePos.z - g_SSAOBias ? 1.0 : 0.0) * rangeCheck;
    }

    float ao = 1.0 - saturate(occlusion / 12.0) * g_SSAOIntensity;
    ao = pow(saturate(ao), g_SSAOPower);

    return float4(ao, ao, ao, 1.0);
}
