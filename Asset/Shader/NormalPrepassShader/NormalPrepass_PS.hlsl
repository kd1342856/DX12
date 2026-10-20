#include "NormalPrepass.hlsli"

float4 main(VSOutput In) : SV_Target0
{
    float roughness = g_metallicRoughnessMap_white.Sample(g_ss_aniso_wrap, In.UV).g * g_roughnessFactor;
    return float4(normalize(In.vN) * 0.5 + 0.5, saturate(roughness));
}
