#include "../Common/ShaderCore.hlsli"

// SSR(Opaque材質向け)。専用のG-Bufferパスを増やさず、SSAOと同じNormalPrepassの
// ビュー空間法線+深度と、Opaqueパスのカラーコピー(既存の屈折用バッファ)を再利用する。
// 粗さ/金属度の情報が無いため、非金属(F0=0.04)前提のフレネル近似のみで、
// 鏡面反射をそのまま加算する(粗さによるボケは無し)。
cbuffer cbSSROpaque : register(b2)
{
    float g_SSROpaqueStepSize;
    float g_SSROpaqueIntensity;
    float2 g_PadSSROpaque;
};

Texture2D g_texNormal : register(t0);      // NormalPrepassのビュー空間法線
Texture2D g_texDepth : register(t1);       // NormalPrepassの深度(このRT自身の専用DepthBuffer)
Texture2D g_texSceneColor : register(t2);  // Opaqueパスのカラーコピー

struct PSInput
{
    float4 Pos : SV_Position;
    float2 UV : TEXCOORD0;
};

float3 ViewPosFromDepth(float2 uv, float depth)
{
    float2 ndc = uv * float2(2.0, -2.0) + float2(-1.0, 1.0);
    float4 clip = float4(ndc, depth, 1.0);
    float4 viewPos = mul(clip, g_mInvP);
    return viewPos.xyz / viewPos.w;
}

float4 main(PSInput In) : SV_Target0
{
    float depth = g_texDepth.Sample(g_ss_point_clamp, In.UV).r;
    if (depth >= 0.9999) return float4(0, 0, 0, 0);

    float3 viewPos = ViewPosFromDepth(In.UV, depth);
    float3 viewNormal = normalize(g_texNormal.Sample(g_ss_point_clamp, In.UV).rgb * 2.0 - 1.0);

    float3 viewDir = normalize(viewPos);
    float3 reflectDir = reflect(viewDir, viewNormal);
    if (reflectDir.z <= 0.0) return float4(0, 0, 0, 0);

    const int kMaxSteps = 24;
    float3 rayPos = viewPos;
    float3 hitColor = 0;
    bool hit = false;

    [loop]
    for (int i = 0; i < kMaxSteps; i++)
    {
        rayPos += reflectDir * g_SSROpaqueStepSize;

        float4 clip = mul(float4(rayPos, 1.0), g_mP);
        if (clip.w <= 0.0001) break;
        float2 ndc = clip.xy / clip.w;
        float2 uv = ndc * float2(0.5, -0.5) + 0.5;
        if (any(uv < 0.0) || any(uv > 1.0)) break;

        float sceneDepthNdc = g_texDepth.Sample(g_ss_point_clamp, uv).r;
        float sceneViewZ = ViewPosFromDepth(uv, sceneDepthNdc).z;

        // 左手系: Zが大きいほど遠い。実際の表面が理論上のレイ位置より手前にあれば、
        // その間に遮蔽物(=反射先)があるということ。
        if (rayPos.z >= sceneViewZ)
        {
            float2 edgeFade = saturate((0.5 - abs(uv - 0.5)) * 8.0);
            float fade = edgeFade.x * edgeFade.y;
            if (fade <= 0.0) break;

            hitColor = g_texSceneColor.Sample(g_ss_linear_clamp, uv).rgb * fade;
            hit = true;
            break;
        }
    }

    if (!hit) return float4(0, 0, 0, 0);

    // 非金属のフレネル近似(F0=0.04)。粗さ/金属度が分からないので鏡面反射のみの近似。
    float NdotV = saturate(dot(-viewDir, viewNormal));
    float fresnel = 0.04 + (1.0 - 0.04) * pow(1.0 - NdotV, 5.0);

    return float4(hitColor * fresnel * g_SSROpaqueIntensity, 1.0);
}
