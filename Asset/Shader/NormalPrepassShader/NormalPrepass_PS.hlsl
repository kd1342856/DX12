#include "NormalPrepass.hlsli"

float4 main(VSOutput In) : SV_Target0
{
    return float4(normalize(In.vN) * 0.5 + 0.5, 1.0);
}
